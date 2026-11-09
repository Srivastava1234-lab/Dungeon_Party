#include "NetClient.h"

#include <utility>

#include "Protocol.h"

NetClient::NetClient() {
    // A client host: no incoming connections, one outgoing connection to the server.
    host = enet_host_create(nullptr, 1, net::CHANNEL_COUNT, 0, 0);
    if (host) host->maximumPacketSize = net::MAX_PACKET_BYTES;
    else error = "Could not create network host";
}

NetClient::~NetClient() {
    if (server) {
        // Try to say goodbye so the server frees the seat right away instead of waiting for a timeout.
        enet_peer_disconnect_now(server, 0);
    }
    if (host) enet_host_destroy(host);
}

bool NetClient::connect(const std::string& serverAddress, std::uint16_t port) {
    if (!host) return false;
    if (server) disconnect();

    ENetAddress address{};
    // Resolves domain names too (e.g. "dungeon.example.com"). This is the only call that may block briefly.
    if (enet_address_set_host(&address, serverAddress.c_str()) != 0) {
        error = "Unknown server address: " + serverAddress;
        return false;
    }
    address.port = port;

    server = enet_host_connect(host, &address, net::CHANNEL_COUNT, net::PROTOCOL_VERSION);
    if (!server) {
        error = "Could not start connecting";
        return false;
    }
    enet_peer_timeout(server, 0, 5000, 10000); // give up after ~5-10 s instead of ENet's default 30 s
    error.clear();
    inbox.clear();
    currentStatus = Status::Connecting;
    return true;
}

void NetClient::disconnect() {
    if (server) {
        enet_peer_disconnect_now(server, 0);
        server = nullptr;
    }
    currentStatus = Status::Disconnected;
}

void NetClient::poll() {
    if (!host) return;
    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) { // 0 = don't wait
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                currentStatus = Status::Connected;
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                error = (currentStatus == Status::Connecting) ? "Could not reach the server"
                                                              : "Lost connection to the server";
                server = nullptr;
                currentStatus = Status::Disconnected;
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                auto message = nlohmann::json::parse(event.packet->data,
                                                     event.packet->data + event.packet->dataLength,
                                                     nullptr, false);
                if (!message.is_discarded() && message.is_object()) inbox.push_back(std::move(message));
                enet_packet_destroy(event.packet);
                break;
            }
            default: break;
        }
    }
}

bool NetClient::send(const nlohmann::json& message) {
    if (currentStatus != Status::Connected || !server) return false;
    const std::string text = message.dump();
    ENetPacket* packet = enet_packet_create(text.data(), text.size(), ENET_PACKET_FLAG_RELIABLE);
    if (enet_peer_send(server, 0, packet) < 0) {
        enet_packet_destroy(packet);
        return false;
    }
    enet_host_flush(host); // send now rather than on the next poll
    return true;
}

std::vector<nlohmann::json> NetClient::takeMessages() {
    return std::exchange(inbox, {});
}
