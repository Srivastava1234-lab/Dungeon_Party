#ifndef DUNGEON_PARTY_NET_CLIENT_H
#define DUNGEON_PARTY_NET_CLIENT_H

// NetClient
// ---------
// A non-blocking connection to dungeon_server. Call poll() once per frame (it never waits),
// then takeMessages() to get what the server sent. This fits straight into the ImGui loop,
// no threads needed.

#include <cstdint>
#include <string>
#include <vector>

#include <enet/enet.h>
#include <nlohmann/json.hpp>

#include "EnetLibrary.h"

class NetClient {
public:
    enum class Status { Disconnected, Connecting, Connected };

    NetClient();
    ~NetClient();
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    // Starts connecting (returns immediately). 'serverAddress' is an IP or a domain name.
    // Returns false if the address can't be resolved; see lastError().
    bool connect(const std::string& serverAddress, std::uint16_t port);
    void disconnect();

    // Handles network events without waiting. Call once per frame.
    void poll();

    // Sends one message to the server. Returns false when not connected.
    bool send(const nlohmann::json& message);

    // Everything received since the last call, oldest first.
    std::vector<nlohmann::json> takeMessages();

    // This computer's address in its local network (e.g. "192.168.178.40"), or "" if unknown.
    // Only reachable for players in the SAME network; see net/PROTOCOL.md for other networks.
    std::string localNetworkAddress() const;

    Status status() const { return currentStatus; }
    const std::string& lastError() const { return error; }

private:
    net::EnetLibrary enetLibrary; // must outlive 'host'
    ENetHost* host = nullptr;
    ENetPeer* server = nullptr;
    Status currentStatus = Status::Disconnected;
    std::string error;
    std::vector<nlohmann::json> inbox;
};

#endif
