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

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <enet/enet.h>
#include <nlohmann/json.hpp>

#include "../net/EnetLibrary.h"
#include "GameData.h"
#include "Match.h"

class RoomServer {
public:
    // Listens on all network interfaces on 'port'. Throws std::runtime_error if the port is taken.
    RoomServer(const GameData& gameData, std::uint16_t port);
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
        std::string heroId;       // which hero from heroes.json this member plays
    };

    struct Room {
        std::string code;
        std::string hostId;
        std::vector<Member> members;
        std::unique_ptr<Match> match; // nullptr while waiting in the lobby
        int nextMemberNumber = 1;
    };

    struct PeerInfo {
        std::string roomCode; // empty = not in a room yet
        std::string memberId;
        int wrongCodes = 0;
    };

    net::EnetLibrary enetLibrary; // must be constructed before 'host' and destroyed after it
    ENetHost* host = nullptr;
    const GameData& gameData;
    std::mt19937 rng;
    std::map<std::string, Room> rooms;                 // code -> room
    std::unordered_map<ENetPeer*, PeerInfo> peers;     // every connected peer

    // ENet events
    void onConnect(ENetPeer* peer);
    void onDisconnect(ENetPeer* peer);
    void onPacket(ENetPeer* peer, const ENetPacket* packet);

    // Requests
    void handleCreate(ENetPeer* peer, const nlohmann::json& message);
    void handleJoin(ENetPeer* peer, const nlohmann::json& message);
    void handleStart(ENetPeer* peer, Room& room, Member& member);
    void handlePlay(ENetPeer* peer, Room& room, Member& member, const nlohmann::json& message);
    void handleEnd(ENetPeer* peer, Room& room, Member& member);

    // Helpers
    void leaveRoom(ENetPeer* peer);
    Room* roomOf(ENetPeer* peer);
    Member* memberOf(Room& room, ENetPeer* peer);
    std::string newRoomCode();
    std::unique_ptr<Match> buildMatch(const Room& room);
    void skipBotAndAbsentTurns(Room& room);

    void send(ENetPeer* peer, const nlohmann::json& message);
    void sendError(ENetPeer* peer, const std::string& text);
    void broadcastLobby(const Room& room);
    void broadcastState(const Room& room);
    nlohmann::json stateFor(const Room& room, const Member& viewer) const;
};

#endif
