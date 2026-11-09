#include "RoomServer.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "../net/Protocol.h"
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

    if (type == net::msg::START) return handleStart(peer, *room, *member);
    if (type == net::msg::PLAY) return handlePlay(peer, *room, *member, message);
    if (type == net::msg::END) return handleEnd(peer, *room, *member);
    sendError(peer, "Unknown message type");
}

// ===================== Lobby =====================

void RoomServer::handleCreate(ENetPeer* peer, const json& message) {
    if (roomOf(peer)) return sendError(peer, "You are already in a room");

    Room room;
    room.code = newRoomCode();
    Member host{peer, "p" + std::to_string(room.nextMemberNumber++), net::sanitizeName(readString(message, "name")), "warrior"};
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

    Member member{peer, "p" + std::to_string(room.nextMemberNumber++), net::sanitizeName(readString(message, "name")), "warrior"};
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

// ===================== Match =====================

void RoomServer::handleStart(ENetPeer* peer, Room& room, Member& member) {
    if (member.id != room.hostId) return sendError(peer, "Only the host can start the match");
    if (room.match) return sendError(peer, "The match has already started");

    try {
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

    // One hero per member. The Hero's playerId is the member id, which is how we
    // find out whose turn it is.
    for (const auto& member : room.members) {
        const HeroDefinition& heroDef = gameData.getHero(member.heroId);
        auto hero = std::make_unique<Hero>(member.id, member.name, heroDef.toHeroType(), heroDef.maxMana);
        for (const auto& entry : heroDef.startingDeck) {
            const CardDefinition& card = gameData.getCard(entry.cardId);
            if (!card.heroEffect) continue;
            for (int i = 0; i < entry.count; ++i)
                hero->addToDeck(std::make_unique<HeroCard>(card.name, card.manaCost, *card.heroEffect, card.value));
        }
        match->addPlayer(std::move(hero));
    }

    // Sprint 2 prototype: the server plays the Dungeon Master as a bot that just ends its turn.
    // A human DM is one more Member with a "role" field - see the notes in net/PROTOCOL.md.
    match->addPlayer(std::make_unique<DungeonMaster>("dm", "Dungeon Master", 10, true));

    const int heroCount = static_cast<int>(room.members.size());
    for (const auto& entry : level.monsters) {
        const MonsterDefinition& monsterDef = gameData.getMonster(entry.monsterId);
        for (int i = 0; i < entry.countPerHero * heroCount; ++i)
            match->addMonster(std::make_unique<Monster>(monsterDef.name, monsterDef.maxHealth, monsterDef.boss));
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
    const auto& monsters = room.match->getMonsters();
    if (targetIndex >= monsters.size()) return sendError(peer, "No monster at that position");

    try {
        room.match->apply(active->getHandCard(cardIndex), *monsters[targetIndex]); // Match validates the rest
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
    for (const auto& m : room.members) players.push_back({{"id", m.id}, {"name", m.name}});
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
                  {"round", match.getRound()}, {"phase", phaseName(match.getPhase())}};

    const Player* active = match.getActivePlayer();
    state["active"] = active ? json(active->getPlayerId()) : json(nullptr);
    if (auto winner = match.getWinner())
        state["winner"] = (*winner == Side::HEROES) ? "HEROES" : "DUNGEON_MASTER";
    else
        state["winner"] = nullptr;

    json heroes = json::array();
    for (const Hero* hero : match.getHeroes()) {
        bool connected = false;
        std::string heroId;
        for (const auto& m : room.members)
            if (m.id == hero->getPlayerId()) { connected = m.peer != nullptr; heroId = m.heroId; }
        heroes.push_back({{"id", hero->getPlayerId()}, {"name", hero->getName()}, {"heroId", heroId},
                          {"hp", hero->getHealth()}, {"maxHp", hero->getType().maxHealth},
                          {"mana", hero->getMana()}, {"maxMana", hero->getMaxMana()},
                          {"handSize", hero->getHandSize()}, {"connected", connected}});

        // Hidden information: only the viewer's own cards are sent to them.
        if (hero->getPlayerId() == viewer.id) {
            json hand = json::array();
            for (std::size_t i = 0; i < hero->getHandSize(); ++i) {
                const Card& card = hero->getHandCard(i);
                const auto* heroCard = dynamic_cast<const HeroCard*>(&card);
                hand.push_back({{"name", card.getName()}, {"cost", card.getManaCost()},
                                {"effect", heroCard ? effectName(heroCard->getEffect()) : "ATTACK"},
                                {"value", heroCard ? heroCard->getValue() : 0}});
            }
            state["hand"] = hand;
            state["cardsPlayed"] = hero->getCardsPlayedThisTurn();
        }
    }
    state["heroes"] = heroes;

    json monsters = json::array();
    for (const auto& monster : match.getMonsters())
        monsters.push_back({{"name", monster->getName()}, {"hp", monster->getHealth()},
                            {"maxHp", monster->getMaxHealth()}, {"alive", monster->isAlive()}});
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
