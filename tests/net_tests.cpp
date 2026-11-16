// End-to-end test: a real RoomServer and real NetClients talking over localhost (UDP).
// Everything runs in one thread; pump() alternates between polling the server and the clients.

#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../net/NetClient.h"
#include "../net/Protocol.h"
#include "../server/RoomServer.h"
#include "GameData.h"

using nlohmann::json;

namespace {

constexpr std::uint16_t TEST_PORT = 47777;

struct TestClient {
    NetClient net;
    std::vector<json> received; // everything this client got, in order

    json lastOfType(const std::string& type) const {
        for (auto it = received.rbegin(); it != received.rend(); ++it)
            if ((*it)["t"] == type) return *it;
        return nullptr;
    }
    int countOfType(const std::string& type) const {
        int n = 0;
        for (const auto& m : received) if (m["t"] == type) ++n;
        return n;
    }
};

void pump(RoomServer& server, std::vector<TestClient*> clients, const std::function<bool()>& done, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        server.poll(1);
        for (auto* c : clients) {
            c->net.poll();
            for (auto& m : c->net.takeMessages()) c->received.push_back(std::move(m));
        }
        if (done()) return;
    }
    throw std::runtime_error("Timed out waiting for the network");
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("FAILED: " + what);
    std::cout << "  ok  " << what << "\n";
}

} // namespace

int main() {
    try {
        GameData data(GameData::defaultDataDirectory());
        RoomServer server(data, TEST_PORT);

        TestClient host, guest, stranger;
        std::vector<TestClient*> all{&host, &guest, &stranger};
        for (auto* c : all) c->net.connect("127.0.0.1", TEST_PORT);
        pump(server, all, [&] {
            for (auto* c : all) if (c->net.status() != NetClient::Status::Connected) return false;
            return true;
        });
        check(true, "three clients connect");

        // ---- Host creates a room and gets a code
        host.net.send({{"t", net::msg::CREATE}, {"name", "Akshat"}});
        pump(server, all, [&] { return !host.lastOfType(net::msg::LOBBY).is_null(); });
        const std::string code = host.lastOfType(net::msg::JOINED)["code"];
        check(code.size() == net::ROOM_CODE_LENGTH, "host receives a 6-character room code: " + code);

        // ---- Wrong code is rejected
        guest.net.send({{"t", net::msg::JOIN}, {"name", "Kagan"}, {"code", "AAAAAA"}});
        pump(server, all, [&] { return !guest.lastOfType(net::msg::ERROR).is_null(); });
        check(guest.lastOfType(net::msg::JOINED).is_null(), "wrong code does not let the guest in");

        // ---- Right code (typed lower-case with a dash) works
        std::string typed = code.substr(0, 3) + "-" + code.substr(3);
        for (auto& ch : typed) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        guest.net.send({{"t", net::msg::JOIN}, {"name", "Kagan"}, {"code", typed}});
        pump(server, all, [&] {
            auto lobby = host.lastOfType(net::msg::LOBBY);
            return !lobby.is_null() && lobby["players"].size() == 2;
        });
        check(guest.lastOfType(net::msg::JOINED)["code"] == code, "guest joins with code typed as '" + typed + "'");
        const std::string guestId = guest.lastOfType(net::msg::JOINED)["you"];

        // ---- A second room is completely separate
        stranger.net.send({{"t", net::msg::CREATE}, {"name", "Stranger"}});
        pump(server, all, [&] { return !stranger.lastOfType(net::msg::JOINED).is_null(); });
        check(stranger.lastOfType(net::msg::JOINED)["code"] != code, "second room gets a different code");
        check(server.roomCount() == 2, "server has two rooms");
        check(stranger.lastOfType(net::msg::LOBBY)["players"].size() == 1, "stranger's lobby only shows the stranger");

        // ---- Only the host can start
        guest.received.clear();
        guest.net.send({{"t", net::msg::START}});
        pump(server, all, [&] { return !guest.lastOfType(net::msg::ERROR).is_null(); });
        check(guest.lastOfType(net::msg::STATE).is_null(), "guest cannot start the match");

        host.net.send({{"t", net::msg::START}});
        pump(server, all, [&] { return !host.lastOfType(net::msg::STATE).is_null() && !guest.lastOfType(net::msg::STATE).is_null(); });
        json hostState = host.lastOfType(net::msg::STATE);
        check(hostState["active"] == hostState["you"], "match starts on the host's turn");
        check(stranger.countOfType(net::msg::STATE) == 0, "other room receives nothing from this match");
        check(hostState["hand"].size() >= 1, "host sees their own hand");
        // Hidden information: each client's "hand" matches THEIR hero's hand size, never someone else's.
        json guestState = guest.lastOfType(net::msg::STATE);
        check(guestState["you"] != hostState["you"] &&
              guestState["hand"].size() == guestState["heroes"][1]["handSize"].get<std::size_t>() &&
              hostState["hand"].size() == hostState["heroes"][0]["handSize"].get<std::size_t>(),
              "each player receives only their own hand");

        // ---- Out-of-turn play is rejected by the server
        guest.received.clear();
        guest.net.send({{"t", net::msg::PLAY}, {"card", 0}, {"target", 0}});
        pump(server, all, [&] { return !guest.lastOfType(net::msg::ERROR).is_null(); });
        check(guest.lastOfType(net::msg::ERROR)["msg"] == "It is not your turn", "guest cannot play on the host's turn");

        // ---- Host attacks the monster
        const int monsterHpBefore = hostState["monsters"][0]["hp"];
        host.received.clear();
        host.net.send({{"t", net::msg::PLAY}, {"card", 0}, {"target", 0}});
        pump(server, all, [&] { return !host.lastOfType(net::msg::STATE).is_null() || !host.lastOfType(net::msg::ERROR).is_null(); });
        check(host.lastOfType(net::msg::ERROR).is_null(), "host's card play is accepted");
        check(host.lastOfType(net::msg::STATE)["monsters"][0]["hp"] < monsterHpBefore, "monster loses health");
        pump(server, all, [&] { return guest.lastOfType(net::msg::STATE)["monsters"][0]["hp"] < monsterHpBefore; });
        check(true, "guest sees the same monster health");

        // ---- Malformed input doesn't crash the server
        host.received.clear();
        host.net.send(json::array({1, 2, 3}));
        pump(server, all, [&] { return !host.lastOfType(net::msg::ERROR).is_null(); });
        check(true, "server rejects a malformed message and keeps running");

        // ---- Turn passes to the guest
        host.received.clear();
        host.net.send({{"t", net::msg::END}});
        pump(server, all, [&] { return !host.lastOfType(net::msg::STATE).is_null(); });
        check(host.lastOfType(net::msg::STATE)["active"] == guestId, "after End turn it is the guest's turn");

        // ---- Guest leaves mid-turn: their turn and the bot DM's are skipped
        host.received.clear();
        guest.net.disconnect();
        pump(server, all, [&] {
            auto s = host.lastOfType(net::msg::STATE);
            return !s.is_null() && s["active"] == hostState["you"];
        }, 12000);
        auto afterLeave = host.lastOfType(net::msg::STATE);
        check(afterLeave["round"] == 2, "game continues in round 2 on the host's turn");
        check(afterLeave["heroes"][1]["connected"] == false, "guest's hero is shown as disconnected");

        // ---- Code guessing gets the peer kicked
        TestClient guesser;
        std::vector<TestClient*> withGuesser{&host, &stranger, &guesser};
        guesser.net.connect("127.0.0.1", TEST_PORT);
        pump(server, withGuesser, [&] { return guesser.net.status() == NetClient::Status::Connected; });
        for (int i = 0; i < net::MAX_WRONG_CODES; ++i)
            guesser.net.send({{"t", net::msg::JOIN}, {"name", "Guesser"}, {"code", "ZZZZZ" + std::to_string(i + 2)}});
        pump(server, withGuesser, [&] { return guesser.net.status() == NetClient::Status::Disconnected; });
        check(true, "peer is disconnected after " + std::to_string(net::MAX_WRONG_CODES) + " wrong codes");

        // ---- Empty rooms are cleaned up
        stranger.net.disconnect();
        pump(server, withGuesser, [&] { return server.roomCount() == 1; }, 12000);
        check(true, "room closes when its last player leaves");

        // ===== Roles: a human Dungeon Master =====
        TestClient heroPlayer, dmPlayer, lateDm;
        std::vector<TestClient*> roles{&heroPlayer, &dmPlayer, &lateDm};
        for (auto* c : roles) c->net.connect("127.0.0.1", TEST_PORT);
        pump(server, roles, [&] {
            for (auto* c : roles) if (c->net.status() != NetClient::Status::Connected) return false;
            return true;
        });
        heroPlayer.net.send({{"t", net::msg::CREATE}, {"name", "Healer"}});
        pump(server, roles, [&] { return !heroPlayer.lastOfType(net::msg::JOINED).is_null(); });
        const std::string roleCode = heroPlayer.lastOfType(net::msg::JOINED)["code"];
        dmPlayer.net.send({{"t", net::msg::JOIN}, {"name", "Boss"}, {"code", roleCode}});
        lateDm.net.send({{"t", net::msg::JOIN}, {"name", "Late"}, {"code", roleCode}});
        pump(server, roles, [&] { return !dmPlayer.lastOfType(net::msg::JOINED).is_null() && !lateDm.lastOfType(net::msg::JOINED).is_null(); });

        heroPlayer.net.send({{"t", net::msg::ROLE}, {"role", "wizard_that_does_not_exist"}});
        pump(server, roles, [&] { return !heroPlayer.lastOfType(net::msg::ERROR).is_null(); });
        check(true, "unknown role is rejected");

        heroPlayer.net.send({{"t", net::msg::ROLE}, {"role", "healer"}});
        dmPlayer.net.send({{"t", net::msg::ROLE}, {"role", "dm"}});
        pump(server, roles, [&] {
            auto lobby = heroPlayer.lastOfType(net::msg::LOBBY);
            int set = 0;
            if (!lobby.is_null()) for (const auto& pl : lobby["players"]) if (pl["role"] != "") ++set;
            return set == 2;
        });
        lateDm.received.clear();
        lateDm.net.send({{"t", net::msg::ROLE}, {"role", "dm"}});
        pump(server, roles, [&] { return !lateDm.lastOfType(net::msg::ERROR).is_null(); });
        check(lateDm.lastOfType(net::msg::ERROR)["msg"] == "Boss is already the Dungeon Master", "only one player can be DM");
        lateDm.net.send({{"t", net::msg::ROLE}, {"role", "warrior"}});
        pump(server, roles, [&] {
            auto lobby = lateDm.lastOfType(net::msg::LOBBY);
            return !lobby.is_null() && lobby["players"][2]["role"] == "warrior";
        });

        heroPlayer.net.send({{"t", net::msg::START}});
        pump(server, roles, [&] { return !dmPlayer.lastOfType(net::msg::STATE).is_null() && !lateDm.lastOfType(net::msg::STATE).is_null(); });
        json dmView = dmPlayer.lastOfType(net::msg::STATE);
        check(dmView["role"] == "dm" && dmView["dm"]["human"] == true, "DM player is the Dungeon Master, not a hero");
        check(dmView["heroes"].size() == 2, "only the two hero players are heroes");
        check(dmView["heroes"][0]["heroId"] == "healer", "healer role gives the healer hero");

        // Heroes take their turns
        for (auto* hero : {&heroPlayer, &lateDm}) {
            hero->received.clear();
            hero->net.send({{"t", net::msg::END}});
            pump(server, roles, [&] { return !hero->lastOfType(net::msg::STATE).is_null(); });
        }
        pump(server, roles, [&] { return dmPlayer.lastOfType(net::msg::STATE)["active"] == dmPlayer.lastOfType(net::msg::STATE)["you"]; });
        dmView = dmPlayer.lastOfType(net::msg::STATE);
        check(dmView["hand"].size() >= 1 && dmView["dm"].contains("deckSize"), "DM sees their hand and deck size on their turn");
        // The DM deck also has Weakening Curse; pick an attack card so the hero takes damage.
        int attackCard = -1;
        for (std::size_t i = 0; i < dmView["hand"].size(); ++i)
            if (dmView["hand"][i]["effect"] == "DM_ATTACK" && attackCard < 0) attackCard = static_cast<int>(i);
        check(attackCard >= 0, "DM has an attack card in hand");

        // A hero cannot give monster orders
        heroPlayer.received.clear();
        heroPlayer.net.send({{"t", net::msg::COMMAND}, {"card", 0}, {"monster", 0}, {"hero", 0}});
        pump(server, roles, [&] { return !heroPlayer.lastOfType(net::msg::ERROR).is_null(); });
        check(true, "heroes cannot command monsters");

        // DM orders monster 0 to attack hero 0, then ends the turn: the hero takes damage
        const int healerHpBefore = dmView["heroes"][0]["hp"];
        dmPlayer.received.clear();
        dmPlayer.net.send({{"t", net::msg::COMMAND}, {"card", attackCard}, {"monster", 0}, {"hero", 0}});
        pump(server, roles, [&] { return !dmPlayer.lastOfType(net::msg::STATE).is_null() || !dmPlayer.lastOfType(net::msg::ERROR).is_null(); });
        check(dmPlayer.lastOfType(net::msg::ERROR).is_null(), "DM's monster order is accepted");
        check(dmPlayer.lastOfType(net::msg::STATE)["dm"]["queued"].size() == 1, "the order shows in the DM's queued list");
        dmPlayer.received.clear();
        dmPlayer.net.send({{"t", net::msg::END}});
        pump(server, roles, [&] { return !heroPlayer.lastOfType(net::msg::STATE).is_null() &&
                                          heroPlayer.lastOfType(net::msg::STATE)["round"] == 2; });
        check(heroPlayer.lastOfType(net::msg::STATE)["heroes"][0]["hp"] < healerHpBefore, "the monster attacked the hero the DM chose");

        // ===== Computer DM attacks too =====
        TestClient solo;
        std::vector<TestClient*> soloList{&solo};
        solo.net.connect("127.0.0.1", TEST_PORT);
        pump(server, soloList, [&] { return solo.net.status() == NetClient::Status::Connected; });
        solo.net.send({{"t", net::msg::CREATE}, {"name", "Solo"}});
        pump(server, soloList, [&] { return !solo.lastOfType(net::msg::JOINED).is_null(); });
        solo.net.send({{"t", net::msg::START}});
        pump(server, soloList, [&] { return !solo.lastOfType(net::msg::STATE).is_null(); });
        const int soloHp = solo.lastOfType(net::msg::STATE)["heroes"][0]["hp"];
        solo.received.clear();
        solo.net.send({{"t", net::msg::END}});
        pump(server, soloList, [&] { auto st = solo.lastOfType(net::msg::STATE); return !st.is_null() && st["round"] == 2; });
        check(solo.lastOfType(net::msg::STATE)["heroes"][0]["hp"] < soloHp, "computer DM's monsters attack back");

        // Play until the boss arrives (level_1: round 4)
        for (int r = 0; r < 2; ++r) {
            solo.received.clear();
            solo.net.send({{"t", net::msg::END}});
            pump(server, soloList, [&] { return !solo.lastOfType(net::msg::STATE).is_null(); });
        }
        const json round4 = solo.lastOfType(net::msg::STATE); // keep it alive while looping over it
        bool bossSeen = false;
        for (const auto& m : round4["monsters"]) bossSeen = bossSeen || m["boss"] == true;
        check(bossSeen, "final boss arrives in round 4");

        // ===== Endpoint parsing =====
        {
            auto ep1 = net::parseServerEndpoint("  play.example.com:12345  ", 7777);
            check(ep1.host == "play.example.com" && ep1.port == 12345, "parseServerEndpoint parses host:port");
            auto ep2 = net::parseServerEndpoint("192.168.1.50", 7777);
            check(ep2.host == "192.168.1.50" && ep2.port == 7777, "parseServerEndpoint uses fallback port");
            auto ep3 = net::parseServerEndpoint("[::1]:8888", 7777);
            check(ep3.host == "::1" && ep3.port == 8888, "parseServerEndpoint parses IPv6 [host]:port");
        }

        // ===== Protocol version mismatch is rejected =====
        {
            TestClient wrongVersion;
            std::vector<TestClient*> list{&wrongVersion};
            wrongVersion.net.connect("127.0.0.1", TEST_PORT, net::PROTOCOL_VERSION + 99);
            pump(server, list, [&] { return wrongVersion.net.status() == NetClient::Status::Disconnected; });
            check(!wrongVersion.lastOfType(net::msg::ERROR).is_null(),
                  "client with wrong protocol version receives error and is disconnected");
        }

        // ===== Reconnect mid-match =====
        {
            TestClient p1, p2;
            std::vector<TestClient*> party{&p1, &p2};
            for (auto* c : party) c->net.connect("127.0.0.1", TEST_PORT);
            pump(server, party, [&] {
                return p1.net.status() == NetClient::Status::Connected && p2.net.status() == NetClient::Status::Connected;
            });
            p1.net.send({{"t", net::msg::CREATE}, {"name", "Alice"}});
            pump(server, party, [&] { return !p1.lastOfType(net::msg::JOINED).is_null(); });
            const std::string rCode = p1.lastOfType(net::msg::JOINED)["code"];
            const std::string p1Token = p1.lastOfType(net::msg::JOINED)["token"];
            check(!p1Token.empty(), "host receives a reconnect token");

            p2.net.send({{"t", net::msg::JOIN}, {"name", "Bob"}, {"code", rCode}});
            pump(server, party, [&] { return !p2.lastOfType(net::msg::JOINED).is_null(); });
            const std::string p2Id = p2.lastOfType(net::msg::JOINED)["you"];
            const std::string p2Token = p2.lastOfType(net::msg::JOINED)["token"];
            check(!p2Token.empty() && p2Token != p1Token, "guest receives a distinct reconnect token");

            p1.net.send({{"t", net::msg::START}});
            pump(server, party, [&] { return !p1.lastOfType(net::msg::STATE).is_null() && !p2.lastOfType(net::msg::STATE).is_null(); });

            // Wrong token is rejected
            TestClient impostor;
            std::vector<TestClient*> withImpostor{&p1, &p2, &impostor};
            impostor.net.connect("127.0.0.1", TEST_PORT);
            pump(server, withImpostor, [&] { return impostor.net.status() == NetClient::Status::Connected; });
            impostor.net.send({{"t", net::msg::REJOIN}, {"code", rCode}, {"token", "WRONG_TOKEN"}});
            pump(server, withImpostor, [&] { return !impostor.lastOfType(net::msg::ERROR).is_null(); });
            check(impostor.lastOfType(net::msg::JOINED).is_null(), "wrong reconnect token is rejected");
            impostor.net.disconnect();

            // p2 disconnects mid-match, then rejoins
            p1.received.clear();
            p2.net.disconnect();
            pump(server, party, [&] {
                auto st = p1.lastOfType(net::msg::STATE);
                return !st.is_null() && st["heroes"][1]["connected"] == false;
            });
            check(true, "other player sees p2 marked disconnected");

            TestClient p2Rejoined;
            std::vector<TestClient*> withRejoin{&p1, &p2Rejoined};
            p2Rejoined.net.connect("127.0.0.1", TEST_PORT);
            pump(server, withRejoin, [&] { return p2Rejoined.net.status() == NetClient::Status::Connected; });
            p1.received.clear();
            p2Rejoined.net.send({{"t", net::msg::REJOIN}, {"code", rCode}, {"token", p2Token}});
            pump(server, withRejoin, [&] {
                return !p2Rejoined.lastOfType(net::msg::JOINED).is_null() &&
                       !p2Rejoined.lastOfType(net::msg::STATE).is_null();
            });
            check(p2Rejoined.lastOfType(net::msg::JOINED).value("rejoined", false) == true, "rejoin sends joined with rejoined=true");
            check(p2Rejoined.lastOfType(net::msg::STATE)["you"] == p2Id &&
                  p2Rejoined.lastOfType(net::msg::STATE)["heroes"][1]["connected"] == true,
                  "rejoined player recovers their hero seat and state");

            // Stale peer takeover: p1 reconnects from a second client before old p1 disconnects
            TestClient p1Takeover;
            std::vector<TestClient*> takeoverList{&p1, &p1Takeover, &p2Rejoined};
            p1Takeover.net.connect("127.0.0.1", TEST_PORT);
            pump(server, takeoverList, [&] { return p1Takeover.net.status() == NetClient::Status::Connected; });
            p1Takeover.net.send({{"t", net::msg::REJOIN}, {"code", rCode}, {"token", p1Token}});
            pump(server, takeoverList, [&] { return !p1Takeover.lastOfType(net::msg::STATE).is_null(); });
            check(p1Takeover.lastOfType(net::msg::STATE)["heroes"][0]["connected"] == true,
                  "stale peer takeover re-binds seat to new connection");

            // Explicit LEAVE clears reconnect token
            p2Rejoined.net.send({{"t", net::msg::LEAVE}});
            pump(server, takeoverList, [&] {
                auto st = p1Takeover.lastOfType(net::msg::STATE);
                return !st.is_null() && st["heroes"][1]["connected"] == false;
            });
            p2Rejoined.net.disconnect();
            TestClient p2AfterLeave;
            std::vector<TestClient*> afterLeaveList{&p1Takeover, &p2AfterLeave};
            p2AfterLeave.net.connect("127.0.0.1", TEST_PORT);
            pump(server, afterLeaveList, [&] { return p2AfterLeave.net.status() == NetClient::Status::Connected; });
            p2AfterLeave.net.send({{"t", net::msg::REJOIN}, {"code", rCode}, {"token", p2Token}});
            pump(server, afterLeaveList, [&] { return !p2AfterLeave.lastOfType(net::msg::ERROR).is_null(); });
            check(p2AfterLeave.lastOfType(net::msg::JOINED).is_null(), "explicit leave forfeits reconnect token");
        }

        // ===== All players drop mid-match: paused during grace, pruned after grace =====
        {
            RoomServer graceServer(data, TEST_PORT + 1, std::chrono::seconds(1));
            TestClient soloDrop;
            std::vector<TestClient*> list{&soloDrop};
            soloDrop.net.connect("127.0.0.1", TEST_PORT + 1);
            pump(graceServer, list, [&] { return soloDrop.net.status() == NetClient::Status::Connected; });
            soloDrop.net.send({{"t", net::msg::CREATE}, {"name", "Lonely"}});
            pump(graceServer, list, [&] { return !soloDrop.lastOfType(net::msg::JOINED).is_null(); });
            const std::string gCode = soloDrop.lastOfType(net::msg::JOINED)["code"];
            const std::string gToken = soloDrop.lastOfType(net::msg::JOINED)["token"];
            soloDrop.net.send({{"t", net::msg::START}});
            pump(graceServer, list, [&] { return !soloDrop.lastOfType(net::msg::STATE).is_null(); });
            const int roundBeforeDrop = soloDrop.lastOfType(net::msg::STATE)["round"];
            const std::size_t roomsBeforeDrop = graceServer.roomCount();

            soloDrop.net.disconnect();
            pump(graceServer, list, [&] { return true; });
            for (int i = 0; i < 10; ++i) graceServer.poll(10);
            check(graceServer.roomCount() == roomsBeforeDrop, "active room stays alive during empty-room grace period");

            TestClient soloBack;
            std::vector<TestClient*> backList{&soloBack};
            soloBack.net.connect("127.0.0.1", TEST_PORT + 1);
            pump(graceServer, backList, [&] { return soloBack.net.status() == NetClient::Status::Connected; });
            soloBack.net.send({{"t", net::msg::REJOIN}, {"code", gCode}, {"token", gToken}});
            pump(graceServer, backList, [&] { return !soloBack.lastOfType(net::msg::STATE).is_null(); });
            check(soloBack.lastOfType(net::msg::STATE)["round"] == roundBeforeDrop,
                  "match paused at same round while everyone was disconnected");

            soloBack.net.disconnect();
            pump(graceServer, backList, [&] { return graceServer.roomCount() == 0; }, 3000);
            check(true, "empty active room closes after reconnect grace expires");
        }

        std::cout << "All network tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
