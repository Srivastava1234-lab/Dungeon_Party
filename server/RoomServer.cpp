#include "RoomServer.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "../net/Protocol.h"
#include "DMCard.h"
#include "HeroCard.h"

using nlohmann::json;

namespace {

const char* phaseName(Phase phase) {
    switch (phase) {
        case Phase::ROUND_START: return "ROUND_START";
        case Phase::HERO_TURN: return "HERO_TURN";
        case Phase::DM_TURN: return "DM_TURN";
        case Phase::MONSTER_ACTIONS: return "MONSTER_ACTIONS";
    }
    return "UNKNOWN";
}

const char* effectName(HeroEffect effect) {
    switch (effect) {
        case HeroEffect::ATTACK: return "ATTACK";
        case HeroEffect::HEAL: return "HEAL";
        case HeroEffect::BUFF: return "BUFF";
        case HeroEffect::SPELL: return "SPELL";
        case HeroEffect::SHIELD: return "SHIELD";
        case HeroEffect::FIREBALL: return "FIREBALL";
    }
    return "ATTACK";
}

// Reads an integer field from untrusted JSON without throwing.
bool readIndex(const json& message, const char* key, std::size_t& out) {
    auto it = message.find(key);
    if (it == message.end() || !it->is_number_integer()) return false;
    const auto value = it->get<long long>();
    if (value < 0) return false;
    out = static_cast<std::size_t>(value);
    return true;
}

std::string readString(const json& message, const char* key) {
    auto it = message.find(key);
    return (it != message.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

} // namespace

// ===================== Setup =====================

RoomServer::RoomServer(const GameData& data, std::uint16_t port)
    : gameData(data), rng(std::random_device{}()) {
    ENetAddress address{};
    address.host = ENET_HOST_ANY; // listen on every network interface
    address.port = port;

    host = enet_host_create(&address, net::MAX_CLIENTS, net::CHANNEL_COUNT, 0, 0);
    if (!host) throw std::runtime_error("Could not open UDP port " + std::to_string(port) + " (already in use?)");
    host->maximumPacketSize = net::MAX_PACKET_BYTES;
}

RoomServer::~RoomServer() {
    if (host) enet_host_destroy(host);
}

void RoomServer::poll(std::uint32_t timeoutMs) {
    ENetEvent event;
    // The first call may wait up to timeoutMs; the following calls only drain what is queued.
    while (enet_host_service(host, &event, timeoutMs) > 0) {
        timeoutMs = 0;
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: onConnect(event.peer); break;
            case ENET_EVENT_TYPE_DISCONNECT: onDisconnect(event.peer); break;
            case ENET_EVENT_TYPE_RECEIVE:
                onPacket(event.peer, event.packet);
                enet_packet_destroy(event.packet);
                break;
            default: break;
        }
    }
    enet_host_flush(host);
}

// ===================== ENet events =====================

void RoomServer::onConnect(ENetPeer* peer) {
    peers[peer] = PeerInfo{};
    // Drop peers that stop answering instead of waiting the ENet default of ~30 seconds.
    enet_peer_timeout(peer, 0, 5000, 10000);
}

void RoomServer::onDisconnect(ENetPeer* peer) {
    leaveRoom(peer);
    peers.erase(peer);
}

void RoomServer::onPacket(ENetPeer* peer, const ENetPacket* packet) {
    // Never trust client data: parse without exceptions and check the shape.
    const json message = json::parse(packet->data, packet->data + packet->dataLength, nullptr, false);
    if (message.is_discarded() || !message.is_object()) {
        sendError(peer, "Malformed message");
        return;
    }
    const std::string type = readString(message, "t");

    if (type == net::msg::CREATE) return handleCreate(peer, message);
    if (type == net::msg::JOIN) return handleJoin(peer, message);
    if (type == net::msg::LEAVE) return leaveRoom(peer);

    // Everything else only makes sense inside a room - and only acts on THAT room.
    Room* room = roomOf(peer);
    Member* member = room ? memberOf(*room, peer) : nullptr;
    if (!room || !member) return sendError(peer, "You are not in a room");

    if (type == net::msg::ROLE) return handleRole(peer, *room, *member, message);
    if (type == net::msg::START) return handleStart(peer, *room, *member);
    if (type == net::msg::COMMAND) return handleCommand(peer, *room, *member, message);
    if (type == net::msg::BOSS) return handleBoss(peer, *room, *member, message);
    if (type == net::msg::PLAY) return handlePlay(peer, *room, *member, message);
    if (type == net::msg::END) return handleEnd(peer, *room, *member);
    sendError(peer, "Unknown message type");
}

// ===================== Lobby =====================

void RoomServer::handleCreate(ENetPeer* peer, const json& message) {
    if (roomOf(peer)) return sendError(peer, "You are already in a room");

    Room room;
    room.code = newRoomCode();
    Member host{peer, "p" + std::to_string(room.nextMemberNumber++), net::sanitizeName(readString(message, "name")), ""};
    room.hostId = host.id;
    room.members.push_back(host);

    peers[peer].roomCode = room.code;
    peers[peer].memberId = host.id;
    const std::string code = room.code;
    rooms.emplace(code, std::move(room));

    std::cout << "Room " << code << " created by " << host.name << "\n";
    send(peer, {{"t", net::msg::JOINED}, {"code", code}, {"you", host.id}});
    broadcastLobby(rooms.at(code));
}

void RoomServer::handleJoin(ENetPeer* peer, const json& message) {
    if (roomOf(peer)) return sendError(peer, "You are already in a room");

    const std::string code = net::normalizeRoomCode(readString(message, "code"));
    auto it = rooms.find(code);
    if (it == rooms.end()) {
        // Same message for "wrong" and "doesn't exist", and a strike limit, so codes can't be guessed.
        if (++peers[peer].wrongCodes >= net::MAX_WRONG_CODES) {
            sendError(peer, "Too many wrong room codes");
            enet_peer_disconnect_later(peer, 0);
        } else {
            sendError(peer, "No room with that code");
        }
        return;
    }

    Room& room = it->second;
    if (room.match) return sendError(peer, "That match has already started");
    if (room.members.size() >= net::MAX_PLAYERS_PER_ROOM) return sendError(peer, "That room is full");

    Member member{peer, "p" + std::to_string(room.nextMemberNumber++), net::sanitizeName(readString(message, "name")), ""};
    room.members.push_back(member);
    peers[peer].roomCode = room.code;
    peers[peer].memberId = member.id;

    std::cout << member.name << " joined room " << room.code << "\n";
    send(peer, {{"t", net::msg::JOINED}, {"code", room.code}, {"you", member.id}});
    broadcastLobby(room);
}

void RoomServer::leaveRoom(ENetPeer* peer) {
    Room* room = roomOf(peer);
    auto peerIt = peers.find(peer);
    if (peerIt != peers.end()) {
        peerIt->second.roomCode.clear();
        peerIt->second.memberId.clear();
    }
    if (!room) return;

    auto memberIt = std::find_if(room->members.begin(), room->members.end(),
                                 [&](const Member& m) { return m.peer == peer; });
    if (memberIt == room->members.end()) return;

    if (room->match) {
        // During a match the hero stays in the game; their turns are skipped from now on.
        memberIt->peer = nullptr;
    } else {
        room->members.erase(memberIt);
    }

    const bool anyoneConnected = std::any_of(room->members.begin(), room->members.end(),
                                             [](const Member& m) { return m.peer != nullptr; });
    if (!anyoneConnected) {
        const std::string code = room->code; // copy: erasing by a reference into the erased element is unsafe
        std::cout << "Room " << code << " closed (empty)\n";
        rooms.erase(code);
        return;
    }

    // If the host left, the next connected member becomes host.
    const bool hostStillHere = std::any_of(room->members.begin(), room->members.end(),
                                           [&](const Member& m) { return m.id == room->hostId && m.peer; });
    if (!hostStillHere) {
        for (const auto& m : room->members)
            if (m.peer) { room->hostId = m.id; break; }
    }

    if (room->match) {
        skipBotAndAbsentTurns(*room);
        broadcastState(*room);
    } else {
        broadcastLobby(*room);
    }
}

void RoomServer::handleRole(ENetPeer* peer, Room& room, Member& member, const json& message) {
    if (room.match) return sendError(peer, "The match has already started");
    const std::string role = readString(message, "role");

    if (role == net::DM_ROLE) {
        for (const auto& other : room.members)
            if (other.id != member.id && other.role == net::DM_ROLE)
                return sendError(peer, other.name + " is already the Dungeon Master");
    } else {
        bool knownHero = false;
        for (const auto& hero : gameData.getHeroes()) knownHero = knownHero || hero.id == role;
        if (!knownHero) return sendError(peer, "Unknown role");
    }
    member.role = role;
    broadcastLobby(room);
}

// ===================== Match =====================

void RoomServer::handleStart(ENetPeer* peer, Room& room, Member& member) {
    if (member.id != room.hostId) return sendError(peer, "Only the host can start the match");
    if (room.match) return sendError(peer, "The match has already started");

    try {
        // Players who never picked a role play the level's default hero.
        const std::string defaultHero = gameData.loadLevel(1).defaultHeroes.front();
        for (auto& m : room.members)
            if (m.role.empty()) m.role = defaultHero;
        room.match = buildMatch(room);
    } catch (const std::exception& e) {
        return sendError(peer, std::string("Could not start the match: ") + e.what());
    }
    std::cout << "Room " << room.code << " started with " << room.members.size() << " player(s)\n";
    skipBotAndAbsentTurns(room);
    broadcastState(room);
}

std::unique_ptr<Match> RoomServer::buildMatch(const Room& room) {
    const LevelDefinition level = gameData.loadLevel(1);
    auto match = std::make_unique<Match>(room.code, level.maxRounds);

    // Heroes: everyone who didn't pick DM. No role chosen = the level's default hero.
    int heroCount = 0;
    const Member* dmMember = nullptr;
    for (const auto& member : room.members) {
        if (member.role == net::DM_ROLE) { dmMember = &member; continue; }
        const std::string heroId = member.role.empty() ? level.defaultHeroes.front() : member.role;
        const HeroDefinition& heroDef = gameData.getHero(heroId);

        // The Hero's playerId is the member id - that's how the server knows whose turn it is.
        auto hero = std::make_unique<Hero>(member.id, member.name, heroDef.toHeroType(), heroDef.maxMana);
        for (const auto& entry : heroDef.startingDeck) {
            const CardDefinition& card = gameData.getCard(entry.cardId);
            if (!card.heroEffect) continue;
            for (int i = 0; i < entry.count; ++i) {
                if (*card.heroEffect == HeroEffect::SPELL)
                    hero->addToDeck(std::make_unique<HeroCard>(card.name, card.manaCost, *card.heroEffect, card.value,
                                                               card.target, card.freezeRounds));
                else
                    hero->addToDeck(std::make_unique<HeroCard>(card.name, card.manaCost, *card.heroEffect, card.value));
            }
        }
        match->addPlayer(std::move(hero));
        ++heroCount;
    }
    if (heroCount == 0) throw std::invalid_argument("Someone has to play a hero");

    // Dungeon Master: the player who picked DM, otherwise the computer.
    auto dm = dmMember ? std::make_unique<DungeonMaster>(dmMember->id, dmMember->name, level.dmMaxMana, false)
                       : std::make_unique<DungeonMaster>("dm", "Dungeon Master", level.dmMaxMana, true);
    for (const auto& entry : level.dmDeck) {
        const CardDefinition& card = gameData.getCard(entry.cardId);
        if (!card.dmEffect) continue;
        for (int i = 0; i < entry.count; ++i)
            dm->addToDeck(std::make_unique<DMCard>(card.name, card.manaCost, *card.dmEffect, card.value));
    }
    match->addPlayer(std::move(dm));

    for (const auto& entry : level.monsters) {
        const MonsterDefinition& monsterDef = gameData.getMonster(entry.monsterId);
        for (int i = 0; i < entry.countPerHero * heroCount; ++i)
            match->addMonster(std::make_unique<Monster>(monsterDef.name, monsterDef.maxHealth, monsterDef.boss));
    }

    // Final boss from the level file: arrives in a later round, health scales with the number of heroes.
    if (!level.bossId.empty()) {
        const MonsterDefinition& bossDef = gameData.getMonster(level.bossId);
        const int bossHealth = bossDef.healthPerHero > 0 ? bossDef.healthPerHero * heroCount : bossDef.maxHealth;
        match->configureBoss(std::make_unique<Monster>(bossDef.name, bossHealth, true),
                             level.bossArrivalRound, bossDef.attackPower);
    }

    match->start();
    return match;
}

// Ends turns that nobody at a keyboard can take: the bot DM, and heroes whose player left.
void RoomServer::skipBotAndAbsentTurns(Room& room) {
    if (!room.match) return;
    for (int guard = 0; guard < 64; ++guard) { // guard: never loop forever on a logic bug
        Player* active = room.match->getActivePlayer();
        if (!active) return;
        bool humanCanPlay = false;
        if (!active->isBot()) {
            for (const auto& m : room.members)
                if (m.id == active->getPlayerId() && m.peer) humanCanPlay = true;
        }
        if (humanCanPlay) return;
        if (auto* dm = dynamic_cast<DungeonMaster*>(active); dm && active->isBot())
            playBotDungeonMaster(*room.match, *dm);
        room.match->endTurn(*active);
    }
}

void RoomServer::handlePlay(ENetPeer* peer, Room& room, Member& member, const json& message) {
    if (!room.match) return sendError(peer, "The match has not started");
    Player* active = room.match->getActivePlayer();
    if (!active || active->getPlayerId() != member.id) return sendError(peer, "It is not your turn");

    std::size_t cardIndex = 0, targetIndex = 0;
    if (!readIndex(message, "card", cardIndex) || !readIndex(message, "target", targetIndex))
        return sendError(peer, "play needs 'card' and 'target' numbers");
    if (cardIndex >= active->getHandSize()) return sendError(peer, "No card at that position");

    // "targetType": "hero" for heals/shields, anything else = a monster
    Targetable* target = nullptr;
    if (readString(message, "targetType") == "hero") {
        const auto& heroes = room.match->getHeroes();
        if (targetIndex >= heroes.size()) return sendError(peer, "No hero at that position");
        target = heroes[targetIndex];
    } else {
        const auto& monsters = room.match->getMonsters();
        if (targetIndex >= monsters.size()) return sendError(peer, "No monster at that position");
        target = monsters[targetIndex].get();
    }

    try {
        room.match->apply(active->getHandCard(cardIndex), *target); // Match validates the rest
    } catch (const std::exception& e) {
        return sendError(peer, e.what());
    }
    broadcastState(room);
}

void RoomServer::handleEnd(ENetPeer* peer, Room& room, Member& member) {
    if (!room.match) return sendError(peer, "The match has not started");
    Player* active = room.match->getActivePlayer();
    if (!active || active->getPlayerId() != member.id) return sendError(peer, "It is not your turn");

    room.match->endTurn(*active);
    skipBotAndAbsentTurns(room);
    broadcastState(room);
}

// ---- Dungeon Master

DungeonMaster* RoomServer::activeDungeonMaster(Room& room, const Member& member) {
    if (!room.match) return nullptr;
    Player* active = room.match->getActivePlayer();
    if (!active || active->getPlayerId() != member.id) return nullptr;
    return dynamic_cast<DungeonMaster*>(active);
}

// DM orders: "monster #m attacks hero #h, using DM card #c". The attacks happen when the DM ends the turn.
void RoomServer::handleCommand(ENetPeer* peer, Room& room, Member& member, const json& message) {
    DungeonMaster* dm = activeDungeonMaster(room, member);
    if (!dm) return sendError(peer, "Only the Dungeon Master can do that, on their turn");

    std::size_t cardIndex = 0, monsterIndex = 0, heroIndex = 0;
    if (!readIndex(message, "card", cardIndex) || !readIndex(message, "monster", monsterIndex) ||
        !readIndex(message, "hero", heroIndex))
        return sendError(peer, "command needs 'card', 'monster' and 'hero' numbers");
    const auto& monsters = room.match->getMonsters();
    const auto& heroes = room.match->getHeroes();
    if (cardIndex >= dm->getHandSize()) return sendError(peer, "No card at that position");
    if (monsterIndex >= monsters.size()) return sendError(peer, "No monster at that position");
    if (heroIndex >= heroes.size()) return sendError(peer, "No hero at that position");

    try {
        room.match->queueMonsterAttack(*dm, *monsters[monsterIndex], dm->getHandCard(cardIndex), *heroes[heroIndex]);
    } catch (const std::exception& e) {
        return sendError(peer, e.what());
    }
    broadcastState(room);
}

void RoomServer::handleBoss(ENetPeer* peer, Room& room, Member& member, const json& message) {
    DungeonMaster* dm = activeDungeonMaster(room, member);
    if (!dm) return sendError(peer, "Only the Dungeon Master can do that, on their turn");
    std::size_t heroIndex = 0;
    if (!readIndex(message, "hero", heroIndex)) return sendError(peer, "boss needs a 'hero' number");
    const auto& heroes = room.match->getHeroes();
    if (heroIndex >= heroes.size()) return sendError(peer, "No hero at that position");

    try {
        room.match->queueBossAttack(*dm, *heroes[heroIndex]);
    } catch (const std::exception& e) {
        return sendError(peer, e.what());
    }
    broadcastState(room);
}

// The computer DM: every living monster attacks the weakest hero while there are cards and mana,
// and the boss attacks too once it has arrived.
void RoomServer::playBotDungeonMaster(Match& match, DungeonMaster& dm) {
    auto weakestHero = [&]() -> Hero* {
        Hero* weakest = nullptr;
        for (Hero* hero : match.getHeroes())
            if (hero->isAlive() && (!weakest || hero->getHealth() < weakest->getHealth())) weakest = hero;
        return weakest;
    };

    for (const auto& monster : match.getMonsters()) {
        if (!monster->isAlive() || monster->isBoss()) continue;
        Hero* target = weakestHero();
        if (!target) return;
        bool ordered = false;
        for (std::size_t i = 0; i < dm.getHandSize() && !ordered; ++i) {
            try {
                match.queueMonsterAttack(dm, *monster, dm.getHandCard(i), *target);
                ordered = true;
            } catch (const std::exception&) {
                // not an attack card, not enough mana or card limit reached: try the next card
            }
        }
        if (!ordered) break;
    }
    if (match.canQueueBossAttack()) {
        if (Hero* target = weakestHero()) match.queueBossAttack(dm, *target);
    }
}

// ===================== Sending =====================

void RoomServer::send(ENetPeer* peer, const json& message) {
    if (!peer) return;
    const std::string text = message.dump();
    ENetPacket* packet = enet_packet_create(text.data(), text.size(), ENET_PACKET_FLAG_RELIABLE);
    if (enet_peer_send(peer, 0, packet) < 0) enet_packet_destroy(packet); // ENet only frees it on success
}

void RoomServer::sendError(ENetPeer* peer, const std::string& text) {
    send(peer, {{"t", net::msg::ERROR}, {"msg", text}});
}

void RoomServer::broadcastLobby(const Room& room) {
    json players = json::array();
    for (const auto& m : room.members) players.push_back({{"id", m.id}, {"name", m.name}, {"role", m.role}});
    const json message = {{"t", net::msg::LOBBY}, {"code", room.code}, {"host", room.hostId}, {"players", players}};
    for (const auto& m : room.members) send(m.peer, message); // only this room's members
}

void RoomServer::broadcastState(const Room& room) {
    for (const auto& m : room.members)
        if (m.peer) send(m.peer, stateFor(room, m)); // each member gets their own view
}

json RoomServer::stateFor(const Room& room, const Member& viewer) const {
    const Match& match = *room.match;
    json state = {{"t", net::msg::STATE}, {"code", room.code}, {"you", viewer.id}, {"host", room.hostId},
                  {"role", viewer.role == net::DM_ROLE ? "dm" : "hero"},
                  {"round", match.getRound()}, {"phase", phaseName(match.getPhase())}};

    const Player* active = match.getActivePlayer();
    state["active"] = active ? json(active->getPlayerId()) : json(nullptr);
    if (auto winner = match.getWinner())
        state["winner"] = (*winner == Side::HEROES) ? "HEROES" : "DUNGEON_MASTER";
    else
        state["winner"] = nullptr;

    auto cardsOf = [](const Player& player) {
        json hand = json::array();
        for (std::size_t i = 0; i < player.getHandSize(); ++i) {
            const Card& card = player.getHandCard(i);
            const auto* heroCard = dynamic_cast<const HeroCard*>(&card);
            const auto* dmCard = dynamic_cast<const DMCard*>(&card);
            hand.push_back({{"name", card.getName()}, {"cost", card.getManaCost()},
                            {"effect", heroCard ? effectName(heroCard->getEffect()) : "DM_ATTACK"},
                            {"value", heroCard ? heroCard->getValue() : dmCard ? dmCard->getValue() : 0}});
        }
        return hand;
    };

    json heroes = json::array();
    for (const Hero* hero : match.getHeroes()) {
        bool connected = false;
        std::string heroId;
        for (const auto& m : room.members)
            if (m.id == hero->getPlayerId()) { connected = m.peer != nullptr; heroId = m.role; }
        heroes.push_back({{"id", hero->getPlayerId()}, {"name", hero->getName()}, {"heroId", heroId},
                          {"hp", hero->getHealth()}, {"maxHp", hero->getType().maxHealth},
                          {"mana", hero->getMana()}, {"maxMana", hero->getMaxMana()},
                          {"handSize", hero->getHandSize()}, {"connected", connected}});

        // Hidden information: only the viewer's own cards are sent to them.
        if (hero->getPlayerId() == viewer.id) {
            state["hand"] = cardsOf(*hero);
            state["cardsPlayed"] = hero->getCardsPlayedThisTurn();
        }
    }
    state["heroes"] = heroes;

    // The Dungeon Master (human or computer)
    for (const auto& member : room.members) {
        if (member.role != net::DM_ROLE) continue;
        state["dm"] = {{"id", member.id}, {"name", member.name}, {"human", true}, {"connected", member.peer != nullptr}};
    }
    if (!state.contains("dm")) state["dm"] = {{"id", "dm"}, {"name", "Computer DM"}, {"human", false}, {"connected", true}};
    if (viewer.role == net::DM_ROLE) {
        if (const auto* dm = dynamic_cast<const DungeonMaster*>(active)) {
            state["hand"] = cardsOf(*dm);
            state["dm"]["mana"] = dm->getMana();
            state["dm"]["maxMana"] = dm->getMaxMana();
            state["canBossAttack"] = match.canQueueBossAttack();
        } else {
            state["hand"] = json::array(); // the DM's hand is only shown on the DM's turn
        }
    }

    json monsters = json::array();
    for (const auto& monster : match.getMonsters())
        monsters.push_back({{"name", monster->getName()}, {"hp", monster->getHealth()},
                            {"maxHp", monster->getMaxHealth()}, {"alive", monster->isAlive()},
                            {"boss", monster->isBoss()}});
    state["monsters"] = monsters;
    return state;
}

// ===================== Lookups =====================

RoomServer::Room* RoomServer::roomOf(ENetPeer* peer) {
    auto peerIt = peers.find(peer);
    if (peerIt == peers.end() || peerIt->second.roomCode.empty()) return nullptr;
    auto roomIt = rooms.find(peerIt->second.roomCode);
    return roomIt == rooms.end() ? nullptr : &roomIt->second;
}

RoomServer::Member* RoomServer::memberOf(Room& room, ENetPeer* peer) {
    for (auto& m : room.members)
        if (m.peer == peer) return &m;
    return nullptr;
}

std::string RoomServer::newRoomCode() {
    std::string code;
    do { code = net::generateRoomCode(rng); } while (rooms.count(code)); // unique among open rooms
    return code;
}
