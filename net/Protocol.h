#ifndef DUNGEON_PARTY_NET_PROTOCOL_H
#define DUNGEON_PARTY_NET_PROTOCOL_H

// Shared by dungeon_server and the game client, so both always agree on the protocol.
//
// Every message is one JSON object sent as one reliable ENet packet. The field "t" says
// what kind of message it is. The full list is in net/PROTOCOL.md.
//
// The server is authoritative: clients only send REQUESTS ("play card 0 on monster 1").
// The server checks them with Match::validate/apply and then sends everyone the new state.

#include <cctype>
#include <cstdint>
#include <random>
#include <string>

#ifdef ERROR
#undef ERROR
#endif

namespace net {

constexpr std::uint16_t DEFAULT_PORT = 7777;
constexpr int PROTOCOL_VERSION = 1;

// ENet settings
constexpr std::size_t CHANNEL_COUNT = 1;          // one reliable, ordered channel is enough for a turn-based game
constexpr std::size_t MAX_CLIENTS = 256;          // connections the server accepts at once
constexpr std::size_t MAX_PACKET_BYTES = 16 * 1024; // ENet's default is 32 MB - far too generous for untrusted input

// Room rules
constexpr std::size_t MAX_PLAYERS_PER_ROOM = 6;   // Match allows 1-6 heroes
constexpr std::size_t ROOM_CODE_LENGTH = 6;
constexpr int MAX_WRONG_CODES = 5;                // then the server disconnects the peer (stops code guessing)
constexpr std::size_t MAX_NAME_LENGTH = 16;

// Room codes avoid characters people mix up when reading them out: 0/O, 1/I/L.
// 31^6 = ~887 million possible codes.
inline constexpr char ROOM_CODE_ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

inline std::string generateRoomCode(std::mt19937& rng) {
    constexpr std::size_t alphabetSize = sizeof(ROOM_CODE_ALPHABET) - 1;
    std::uniform_int_distribution<std::size_t> pick(0, alphabetSize - 1);
    std::string code;
    for (std::size_t i = 0; i < ROOM_CODE_LENGTH; ++i) code += ROOM_CODE_ALPHABET[pick(rng)];
    return code;
}

// Upper-cases and strips spaces/dashes, so "abc 123" and "ABC-123" both work when typed in.
inline std::string normalizeRoomCode(const std::string& typed) {
    std::string code;
    for (unsigned char c : typed) {
        if (c == ' ' || c == '-') continue;
        code += static_cast<char>(std::toupper(c));
    }
    return code;
}

// Keeps printable ASCII only and limits the length. Names are shown to other players,
// so the server never trusts what a client sends.
inline std::string sanitizeName(const std::string& raw) {
    std::string name;
    for (unsigned char c : raw) {
        if (c >= 32 && c < 127) name += static_cast<char>(c);
        if (name.size() == MAX_NAME_LENGTH) break;
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    return name.empty() ? "Player" : name;
}

// Message types ("t" field)
namespace msg {
    // client -> server
    inline constexpr const char* CREATE = "create"; // { name }                     host makes a new room
    inline constexpr const char* JOIN   = "join";   // { name, code }               join an existing room
    inline constexpr const char* START  = "start";  // {}                           host only, starts the match
    inline constexpr const char* PLAY   = "play";   // { card, target }             hand index, monster index
    inline constexpr const char* END    = "end";    // {}                           end your turn
    inline constexpr const char* ROLE   = "role";   // { role }                     lobby only: "HERO" or "DM" (one DM per room)
    inline constexpr const char* QUEUE  = "queue";  // { card, monster, target }    DM only: order a monster to attack a hero
    inline constexpr const char* LEAVE  = "leave";  // {}

    // server -> client
    inline constexpr const char* JOINED = "joined"; // { code, you, host }          you are in a room now
    inline constexpr const char* LOBBY  = "lobby";  // { code, players[{id,name,role}] } waiting room changed
    inline constexpr const char* STATE  = "state";  // { round, phase, ... }        match state (only YOUR hand)
    inline constexpr const char* ERROR  = "error";  // { msg }                      request was rejected
    inline constexpr const char* CLOSED = "closed"; // { msg }                      room was closed
}

} // namespace net

#endif // DUNGEON_PARTY_NET_PROTOCOL_H
