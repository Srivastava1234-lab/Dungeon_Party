#ifndef DUNGEON_PARTY_ROOM_SERVER_H
#define DUNGEON_PARTY_ROOM_SERVER_H

// RoomServer
// ----------
// One server process hosts many rooms. Each room has a code (e.g. "K7RM2Q") and is
// completely separate: messages from a room only ever go to that room's members, and a
// peer can only act inside the room it joined. That is the "own virtual network per room".
//
// Every room runs its own Match. Clients never change game state directly - they send a
// request, the server runs it through Match::apply (which validates it) and then sends every
// member the new state. Each member only receives THEIR OWN hand.

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <enet/enet.h>
#include <nlohmann/json.hpp>

#include "../net/EnetLibrary.h"
#include "../net/Protocol.h"
#include "GameData.h"
#include "Match.h"

class RoomServer {
public:
    // Listens on all network interfaces on 'port'. Throws std::runtime_error if the port is taken.
    struct ResultPlayer {
        std::string id;
        std::string name;
        std::string role;
    };
    using ResultSink = std::function<void(const std::string&, Side, const std::vector<ResultPlayer>&)>;

    // Turn time limits come from rules.json; 'turnSecondsOverride' > 0 replaces them for every
    // turn (used by the tests, and handy for a quick demo: dungeon_server 7777 <data> 15).
    // 'emptyRoomGrace': how long an empty room waits for players to reconnect (Taegan).
    RoomServer(const GameData& gameData, std::uint16_t port, int turnSecondsOverride = 0,
               std::chrono::seconds emptyRoomGrace = std::chrono::seconds(net::EMPTY_ROOM_GRACE_SECONDS),
               ResultSink resultSink = {});
    // Only a custom reconnect grace period (used by the reconnect tests)
    RoomServer(const GameData& gameData, std::uint16_t port, std::chrono::seconds emptyRoomGrace)
        : RoomServer(gameData, port, 0, emptyRoomGrace) {}
    ~RoomServer();
    RoomServer(const RoomServer&) = delete;
    RoomServer& operator=(const RoomServer&) = delete;

    // Handles network events for up to 'timeoutMs' milliseconds. Call this in a loop.
    void poll(std::uint32_t timeoutMs);

    std::size_t roomCount() const { return rooms.size(); }

private:
    struct Member {
        ENetPeer* peer = nullptr; // nullptr = disconnected during the match (their turns are skipped)
        std::string id;           // "p1", "p2", ... unique inside the room; also the Hero's playerId
        std::string name;
        std::string role;         // "" = not chosen yet, "dm", or a hero id from heroes.json ("warrior", "healer")
        std::string token;        // secret token allowing this player to reclaim their seat on reconnect
    };

    struct Room {
        std::string code;
        std::string hostId;
        std::vector<Member> members;
        bool resultSaved = false;
        std::unique_ptr<Match> match; // nullptr while waiting in the lobby
        nlohmann::json queued = nlohmann::json::array(); // DM orders this turn {monster, hero, card} (SCRUM-40)

        // Turn timer: whose turn it is ("<playerId>#<round>") and when it ends automatically.
        std::string timedTurn;
        int turnSeconds = 0;                                  // 0 = no limit
        std::chrono::steady_clock::time_point turnDeadline{};
        int nextMemberNumber = 1;
        std::optional<std::chrono::steady_clock::time_point> emptySince; // set when all players drop mid-match
    };

    struct PeerInfo {
        std::string roomCode; // empty = not in a room yet
        std::string memberId;
        int wrongCodes = 0;
    };

    net::EnetLibrary enetLibrary; // must be constructed before 'host' and destroyed after it
    ENetHost* host = nullptr;
    const GameData& gameData;
    ResultSink resultSink;
    std::chrono::seconds emptyRoomGrace;
    std::mt19937 rng;
    int turnSecondsOverride = 0;
    std::map<std::string, Room> rooms;                 // code -> room
    std::unordered_map<ENetPeer*, PeerInfo> peers;     // every connected peer

    // ENet events
    void onConnect(ENetPeer* peer, enet_uint32 protocolVersion);
    void onDisconnect(ENetPeer* peer);
    void onPacket(ENetPeer* peer, const ENetPacket* packet);

    // Requests
    void handleCreate(ENetPeer* peer, const nlohmann::json& message);
    void handleJoin(ENetPeer* peer, const nlohmann::json& message);
    void handleRejoin(ENetPeer* peer, const nlohmann::json& message);
    void handleRole(ENetPeer* peer, Room& room, Member& member, const nlohmann::json& message);
    void handleStart(ENetPeer* peer, Room& room, Member& member);
    void handlePlay(ENetPeer* peer, Room& room, Member& member, const nlohmann::json& message);
    void handleEnd(ENetPeer* peer, Room& room, Member& member);
    void handleCommand(ENetPeer* peer, Room& room, Member& member, const nlohmann::json& message);
    void handleBoss(ENetPeer* peer, Room& room, Member& member, const nlohmann::json& message);

    // Helpers
    void leaveRoom(ENetPeer* peer, bool explicitLeave);
    void pruneExpiredRooms();
    Room* roomOf(ENetPeer* peer);
    Member* memberOf(Room& room, ENetPeer* peer);
    std::string newRoomCode();
    std::unique_ptr<Match> buildMatch(const Room& room);
    void skipBotAndAbsentTurns(Room& room);

    // Turn timer
    bool endTurnIfAllCardsPlayed(Room& room, Player& active); // returns true if it ended the turn
    int turnLimitFor(const Player& player) const;  // seconds; 0 = no limit
    void restartTimerIfTurnChanged(Room& room);    // called before every state broadcast
    void endExpiredTurns();                        // called in every poll()
    int millisecondsLeft(const Room& room) const;
    void playBotDungeonMaster(Match& match, DungeonMaster& dm); // queues attacks for the computer DM
    DungeonMaster* activeDungeonMaster(Room& room, const Member& member);

    void send(ENetPeer* peer, const nlohmann::json& message);
    void sendError(ENetPeer* peer, const std::string& text);
    void broadcastLobby(const Room& room);
    void broadcastState(Room& room);
    void broadcastInfo(const Room& room, const std::string& text);
    nlohmann::json stateFor(const Room& room, const Member& viewer) const;
};

#endif
