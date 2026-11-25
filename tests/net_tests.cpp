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
#include "../server/MatchResultStore.h"
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
        MatchResultStore results(":memory:");
        std::vector<std::string> savedMatchIds;
        RoomServer server(data, TEST_PORT, 0, std::chrono::seconds(10),
                          [&](const auto& id, Side winner, const auto& players) {
                              results.save(id, winner, players);
                              savedMatchIds.push_back(id);
                          });
        server.setLoginRequired(false); // rooms and turns; login is tested below

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
        // Read the limit from rules.json, so changing the timer there doesn't break this test.
        const int heroLimit = data.getRules().heroTurnTimeLimitSeconds;
        check(hostState["turnSeconds"] == heroLimit && hostState["turnMsLeft"].get<int>() > (heroLimit - 5) * 1000 &&
              hostState["turnMsLeft"].get<int>() <= heroLimit * 1000,
              "turn timer uses the hero limit from rules.json (" + std::to_string(heroLimit) + " s)");
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

        // Battle Cry uses the real server deck, hero targeting and public snapshot.
        int buffIndex = -1;
        for (std::size_t i = 0; i < hostState["hand"].size(); ++i)
            if (hostState["hand"][i]["effect"] == "BUFF") buffIndex = static_cast<int>(i);
        check(buffIndex >= 0, "Battle Cry is available in the starting hand");
        host.received.clear();
        host.net.send({{"t", net::msg::PLAY}, {"card", buffIndex}, {"target", 0}, {"targetType", "hero"}});
        pump(server, all, [&] {
            return !host.lastOfType(net::msg::STATE).is_null() || !host.lastOfType(net::msg::ERROR).is_null();
        });
        check(host.lastOfType(net::msg::ERROR).is_null(), "server accepts Battle Cry");
        hostState = host.lastOfType(net::msg::STATE);
        check(hostState["heroes"][0]["buffTurns"] == 2 && hostState["heroes"][0]["buffValue"] == 5,
              "server publishes buff strength and remaining target turns");
        pump(server, all, [&] {
            return guest.lastOfType(net::msg::STATE)["heroes"][0].value("buffTurns", 0) == 2;
        });

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
        check(afterLeave["heroes"][0]["buffTurns"] == 1, "buff ticks on target turn, not round boundary");
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
        lateDm.net.send({{"t", net::msg::ROLE}, {"role", "wizard"}}); // the Wizard is playable now
        pump(server, roles, [&] {
            auto lobby = lateDm.lastOfType(net::msg::LOBBY);
            return !lobby.is_null() && lobby["players"][2]["role"] == "wizard";
        });

        heroPlayer.net.send({{"t", net::msg::START}});
        pump(server, roles, [&] { return !dmPlayer.lastOfType(net::msg::STATE).is_null() && !lateDm.lastOfType(net::msg::STATE).is_null(); });
        json dmView = dmPlayer.lastOfType(net::msg::STATE);
        check(dmView["role"] == "dm" && dmView["dm"]["human"] == true, "DM player is the Dungeon Master, not a hero");
        check(dmView["heroes"].size() == 2, "only the two hero players are heroes");
        check(dmView["heroes"][0]["heroId"] == "healer", "healer role gives the healer hero");
        check(dmView["heroes"][1]["heroId"] == "wizard", "wizard role gives the wizard hero");

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

        // Swarms: rounds pass, but the boss only enters after both swarms are defeated
        for (int r = 0; r < 2; ++r) {
            solo.received.clear();
            solo.net.send({{"t", net::msg::END}});
            pump(server, soloList, [&] { return !solo.lastOfType(net::msg::STATE).is_null(); });
        }
        const json round4 = solo.lastOfType(net::msg::STATE); // keep it alive while looping over it
        bool bossSeen = false;
        for (const auto& m : round4["monsters"]) bossSeen = bossSeen || m["boss"] == true;
        check(round4["round"] == 4 && !bossSeen && round4["bossArrived"] == false,
              "the boss does not arrive after a fixed number of rounds");
        check(round4["swarm"] == 1 && round4["swarms"] == 2, "the state reports swarm 1 of 2");
        check(round4["level"] == 1, "the state reports the level being played");

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
            graceServer.setLoginRequired(false);
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
        // ===== Automatic turn end after the card limit (normal server: 30 s timer) =====
        {
            TestClient quick;
            std::vector<TestClient*> q{&quick};
            quick.net.connect("127.0.0.1", TEST_PORT);
            pump(server, q, [&] { return quick.net.status() == NetClient::Status::Connected; });
            quick.net.send({{"t", net::msg::CREATE}, {"name", "Quick"}});
            pump(server, q, [&] { return !quick.lastOfType(net::msg::JOINED).is_null(); });
            const std::string quickId = quick.lastOfType(net::msg::JOINED)["you"];
            quick.net.send({{"t", net::msg::START}});
            pump(server, q, [&] { return !quick.lastOfType(net::msg::STATE).is_null(); });
            const int limit = data.getRules().maxCardsPlayedPerTurn;

            for (int played = 1; played <= limit; ++played) {
                quick.received.clear();
                quick.net.send({{"t", net::msg::PLAY}, {"card", 0}, {"target", 0}, {"targetType", "monster"}});
                pump(server, q, [&] { return !quick.lastOfType(net::msg::STATE).is_null() || !quick.lastOfType(net::msg::ERROR).is_null(); });
                check(quick.lastOfType(net::msg::ERROR).is_null(), "card " + std::to_string(played) + " is accepted");
                if (played < limit)
                    check(quick.lastOfType(net::msg::STATE)["active"] == quickId && quick.lastOfType(net::msg::STATE)["round"] == 1,
                          "after " + std::to_string(played) + " card(s) it is still the same turn");
            }
            // Only up to 3 s of waiting - far less than the 30 s timer, so the card limit ended the turn.
            pump(server, q, [&] { auto st = quick.lastOfType(net::msg::STATE); return !st.is_null() && st["round"] == 2; });
            check(quick.lastOfType(net::msg::INFO)["msg"] == "Quick played " + std::to_string(limit) + " cards - turn ended automatically",
                  "after the last allowed card the turn ends at once (no waiting for the timer)");
        }

        // ===== Turn timer (a second server where every turn lasts 1 second) =====
        RoomServer fastServer(data, TEST_PORT + 1, 1);
        fastServer.setLoginRequired(false);
        TestClient slowHero, slowGuest, dmTimer, timerHero;
        std::vector<TestClient*> timed{&slowHero, &slowGuest, &dmTimer, &timerHero};
        for (auto* c : timed) c->net.connect("127.0.0.1", TEST_PORT + 1);
        pump(fastServer, timed, [&] {
            for (auto* c : timed) if (c->net.status() != NetClient::Status::Connected) return false;
            return true;
        });

        // Room 1: two heroes who never press anything
        slowHero.net.send({{"t", net::msg::CREATE}, {"name", "Sleepy"}});
        pump(fastServer, timed, [&] { return !slowHero.lastOfType(net::msg::JOINED).is_null(); });
        slowGuest.net.send({{"t", net::msg::JOIN}, {"name", "Dozy"}, {"code", slowHero.lastOfType(net::msg::JOINED)["code"]}});
        pump(fastServer, timed, [&] { return !slowGuest.lastOfType(net::msg::JOINED).is_null(); });
        const std::string sleepyId = slowHero.lastOfType(net::msg::JOINED)["you"];
        const std::string dozyId = slowGuest.lastOfType(net::msg::JOINED)["you"];
        slowHero.net.send({{"t", net::msg::START}});
        pump(fastServer, timed, [&] { return !slowGuest.lastOfType(net::msg::STATE).is_null(); });
        json timedState = slowGuest.lastOfType(net::msg::STATE);
        check(timedState["active"] == sleepyId && timedState["turnSeconds"] == 1 &&
              timedState["turnMsLeft"].get<int>() > 0 && timedState["turnMsLeft"].get<int>() <= 1000,
              "everyone receives the countdown for the active player's turn");

        const auto turnStart = std::chrono::steady_clock::now();
        pump(fastServer, timed, [&] { return slowGuest.lastOfType(net::msg::STATE)["active"] == dozyId; });
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - turnStart).count();
        check(waited >= 800, "the turn is NOT ended early (waited " + std::to_string(waited) + " ms)");
        check(slowHero.lastOfType(net::msg::INFO)["msg"] == "Sleepy ran out of time - turn ended automatically",
              "everyone is told that the turn timed out");
        check(slowGuest.lastOfType(net::msg::STATE)["turnMsLeft"].get<int>() > 500, "the next player gets a fresh timer");

        // The second hero also sleeps: the computer DM plays instantly and round 2 starts with the first hero
        pump(fastServer, timed, [&] {
            auto st = slowHero.lastOfType(net::msg::STATE);
            return st["round"] == 2 && st["active"] == sleepyId;
        });
        check(true, "after both heroes time out, the computer DM plays and round 2 begins");

        // Playing a card does not reset the timer
        slowHero.received.clear();
        slowHero.net.send({{"t", net::msg::PLAY}, {"card", 0}, {"target", 0}, {"targetType", "monster"}});
        pump(fastServer, timed, [&] { return !slowHero.lastOfType(net::msg::STATE).is_null() || !slowHero.lastOfType(net::msg::ERROR).is_null(); });
        if (!slowHero.lastOfType(net::msg::STATE).is_null())
            check(slowHero.lastOfType(net::msg::STATE)["turnMsLeft"].get<int>() < 1000, "playing a card does not give extra time");

        // Room 2: a human DM gives an order and then lets the timer run out - the order still happens
        timerHero.net.send({{"t", net::msg::CREATE}, {"name", "Hero"}});
        pump(fastServer, timed, [&] { return !timerHero.lastOfType(net::msg::JOINED).is_null(); });
        dmTimer.net.send({{"t", net::msg::JOIN}, {"name", "SlowDM"}, {"code", timerHero.lastOfType(net::msg::JOINED)["code"]}});
        pump(fastServer, timed, [&] { return !dmTimer.lastOfType(net::msg::JOINED).is_null(); });
        dmTimer.net.send({{"t", net::msg::ROLE}, {"role", "dm"}});
        pump(fastServer, timed, [&] {
            auto lobby = timerHero.lastOfType(net::msg::LOBBY);
            return !lobby.is_null() && lobby["players"].size() == 2 && lobby["players"][1]["role"] == "dm";
        });
        timerHero.net.send({{"t", net::msg::START}});
        pump(fastServer, timed, [&] { return !timerHero.lastOfType(net::msg::STATE).is_null(); });
        timerHero.received.clear();
        timerHero.net.send({{"t", net::msg::END}});
        pump(fastServer, timed, [&] {
            auto st = dmTimer.lastOfType(net::msg::STATE);
            return !st.is_null() && st["active"] == st["you"] && st["hand"].size() > 0;
        });
        json dmTurn = dmTimer.lastOfType(net::msg::STATE);
        check(dmTurn["turnSeconds"] == 1, "the DM's turn is timed too");
        int slash = -1;
        for (std::size_t i = 0; i < dmTurn["hand"].size(); ++i)
            if (dmTurn["hand"][i]["effect"] == "DM_ATTACK" && slash < 0) slash = static_cast<int>(i);
        const int heroHp = dmTurn["heroes"][0]["hp"];
        dmTimer.net.send({{"t", net::msg::COMMAND}, {"card", slash}, {"monster", 0}, {"hero", 0}});
        pump(fastServer, timed, [&] {
            auto st = timerHero.lastOfType(net::msg::STATE);
            return !st.is_null() && st["round"] == 2;
        });
        check(timerHero.lastOfType(net::msg::STATE)["heroes"][0]["hp"] < heroHp,
              "when the DM times out, the orders already given are still carried out");

        // ===== Timer + reconnect (Taegan): a paused room must not lose turns =====
        {
            RoomServer pausedServer(data, TEST_PORT + 2, 1, std::chrono::seconds(10)); // 1 s turns, 10 s grace
            pausedServer.setLoginRequired(false);
            TestClient leaver;
            std::vector<TestClient*> one{&leaver};
            leaver.net.connect("127.0.0.1", TEST_PORT + 2);
            pump(pausedServer, one, [&] { return leaver.net.status() == NetClient::Status::Connected; });
            leaver.net.send({{"t", net::msg::CREATE}, {"name", "Leaver"}});
            pump(pausedServer, one, [&] { return !leaver.lastOfType(net::msg::JOINED).is_null(); });
            const std::string pCode = leaver.lastOfType(net::msg::JOINED)["code"];
            const std::string pToken = leaver.lastOfType(net::msg::JOINED)["token"];
            leaver.net.send({{"t", net::msg::START}});
            pump(pausedServer, one, [&] { return !leaver.lastOfType(net::msg::STATE).is_null(); });
            const int roundBefore = leaver.lastOfType(net::msg::STATE)["round"];

            leaver.net.disconnect(); // the room is now empty and paused
            const auto pauseStart = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - pauseStart < std::chrono::milliseconds(2500)) pausedServer.poll(5);

            TestClient back;
            std::vector<TestClient*> backList{&back};
            back.net.connect("127.0.0.1", TEST_PORT + 2);
            pump(pausedServer, backList, [&] { return back.net.status() == NetClient::Status::Connected; });
            back.net.send({{"t", net::msg::REJOIN}, {"code", pCode}, {"token", pToken}});
            pump(pausedServer, backList, [&] { return !back.lastOfType(net::msg::STATE).is_null(); });
            const json resumed = back.lastOfType(net::msg::STATE);
            check(resumed["round"] == roundBefore, "turn timer does not run while the room is paused for reconnects");
            check(resumed["turnMsLeft"].get<int>() > 500, "after rejoining, the turn gets a fresh timer");
        }

        // Completed matches use permanent account IDs, independent of room seats and hero roles.
        {
            RoomServer resultServer(data, TEST_PORT + 4, 0, std::chrono::seconds(10),
                                    [&](const auto& id, Side winner, const auto& players) {
                                        results.save(id, winner, players);
                                        savedMatchIds.push_back(id);
                                    });
            std::string heroAccountId, dmAccountId, previousMatchId;
            for (const int level : {1, 2}) {
                TestClient finisher, opponent;
                std::vector<TestClient*> clients{&finisher, &opponent};
                for (auto* client : clients) client->net.connect("127.0.0.1", TEST_PORT + 4);
                pump(resultServer, clients, [&] {
                    for (auto* client : clients)
                        if (client->net.status() != NetClient::Status::Connected) return false;
                    return true;
                });
                const auto authType = level == 1 ? net::msg::REGISTER : net::msg::LOGIN;
                finisher.net.send({{"t", authType}, {"username", "SavedHero"}, {"password", "result-test-123"}});
                opponent.net.send({{"t", authType}, {"username", "SavedDM"}, {"password", "result-test-456"}});
                pump(resultServer, clients, [&] {
                    return !finisher.lastOfType(net::msg::AUTH).is_null() &&
                           !opponent.lastOfType(net::msg::AUTH).is_null();
                });
                const std::string heroId = finisher.lastOfType(net::msg::AUTH)["playerId"];
                const std::string dmId = opponent.lastOfType(net::msg::AUTH)["playerId"];
                check(heroId.rfind("P-", 0) == 0 && dmId.rfind("P-", 0) == 0 && heroId != dmId,
                      "results participants have distinct permanent account IDs");
                if (level == 1) { heroAccountId = heroId; dmAccountId = dmId; }
                else check(heroId == heroAccountId && dmId == dmAccountId,
                           "logging in for another match preserves both player IDs");

                const auto before = savedMatchIds.size();
                finisher.net.send({{"t", net::msg::CREATE}, {"name", "Forged name"}, {"level", level}});
                pump(resultServer, clients, [&] { return !finisher.lastOfType(net::msg::LOBBY).is_null(); });
                check(finisher.lastOfType(net::msg::LOBBY)["level"] == level,
                      "results-enabled server preserves the selected level");
                const std::string roomCode = finisher.lastOfType(net::msg::JOINED)["code"];
                const std::string seatId = finisher.lastOfType(net::msg::JOINED)["you"];
                check(heroId != seatId, "permanent ID is separate from the room's turn-order ID");
                opponent.net.send({{"t", net::msg::JOIN}, {"code", roomCode}});
                pump(resultServer, clients, [&] { return !opponent.lastOfType(net::msg::JOINED).is_null(); });
                const std::string role = level == 1 ? "warrior" : "wizard";
                finisher.net.send({{"t", net::msg::ROLE}, {"role", role}});
                opponent.net.send({{"t", net::msg::ROLE}, {"role", "dm"}});
                pump(resultServer, clients, [&] {
                    auto lobby = finisher.lastOfType(net::msg::LOBBY);
                    return lobby["players"].size() == 2 && lobby["players"][0]["role"] == role &&
                           lobby["players"][1]["role"] == "dm";
                });
                finisher.received.clear();
                finisher.net.send({{"t", net::msg::START}});
                pump(resultServer, clients, [&] { return !finisher.lastOfType(net::msg::STATE).is_null(); });
                check(savedMatchIds.size() == before, "an unfinished match is not saved");
                const int turnLimit = data.loadLevel(level).maxRounds * 2;
                for (int turn = 0; turn < turnLimit && finisher.lastOfType(net::msg::STATE)["winner"].is_null(); ++turn) {
                    auto* active = finisher.lastOfType(net::msg::STATE)["active"] == seatId ? &finisher : &opponent;
                    finisher.received.clear();
                    opponent.received.clear();
                    active->net.send({{"t", net::msg::END}});
                    pump(resultServer, clients, [&] {
                        return !finisher.lastOfType(net::msg::STATE).is_null() ||
                               !active->lastOfType(net::msg::ERROR).is_null();
                    });
                    check(active->lastOfType(net::msg::ERROR).is_null(), "authenticated match progresses normally");
                }
                check(!finisher.lastOfType(net::msg::STATE)["winner"].is_null(), "network match finishes");
                check(savedMatchIds.size() == before + 1, "finished network match is saved once");
                const std::string id = savedMatchIds.back();
                check(id.size() == 32 && id != roomCode && id != previousMatchId,
                      "each completed match gets a unique ID, independent of its room code");
                const auto saved = results.load(id);
                check(saved && saved->winner == Side::DUNGEON_MASTER && saved->players.size() == 2 &&
                          !saved->finishedAtUtc.empty(), "completed match loads with winner, timestamp and both accounts");
                bool foundHero = false, foundDM = false;
                for (const auto& player : saved->players) {
                    foundHero = foundHero || (player.id == heroId && player.name == "SavedHero" && player.role == role);
                    foundDM = foundDM || (player.id == dmId && player.name == "SavedDM" && player.role == "dm");
                }
                check(foundHero && foundDM, "results store permanent account IDs, account names and selected roles");
                if (!previousMatchId.empty()) {
                    const auto previous = results.load(previousMatchId);
                    check(previous && previous->players.size() == 2,
                          "the previous match still loads independently for the same accounts");
                    for (const auto& player : previous->players)
                        if (player.id == heroId) check(player.role == "warrior", "old results retain the hero role from that match");
                }
                previousMatchId = id;
                for (auto* client : clients) client->net.disconnect();
                pump(resultServer, clients, [&] { return resultServer.roomCount() == 0; }, 12000);
            }
        }

        // ===== Login: accounts with a permanent player ID =====
        {
            RoomServer loginServer(data, TEST_PORT + 3); // login required (the default)
            TestClient alice, mallory, bob;
            std::vector<TestClient*> clients{&alice, &mallory, &bob};
            for (auto* c : clients) c->net.connect("127.0.0.1", TEST_PORT + 3);
            pump(loginServer, clients, [&] {
                for (auto* c : clients) if (c->net.status() != NetClient::Status::Connected) return false;
                return true;
            });

            alice.net.send({{"t", net::msg::CREATE}, {"name", "Alice"}});
            pump(loginServer, clients, [&] { return !alice.lastOfType(net::msg::ERROR).is_null(); });
            check(alice.lastOfType(net::msg::ERROR)["msg"] == "Please log in first", "hosting without logging in is refused");

            alice.received.clear();
            alice.net.send({{"t", net::msg::REGISTER}, {"username", "al"}, {"password", "secret123"}});
            pump(loginServer, clients, [&] { return !alice.lastOfType(net::msg::ERROR).is_null(); });
            check(true, "a too short username is refused: " + alice.lastOfType(net::msg::ERROR)["msg"].get<std::string>());

            alice.received.clear();
            alice.net.send({{"t", net::msg::REGISTER}, {"username", "Alice"}, {"password", "secret123"}});
            pump(loginServer, clients, [&] { return !alice.lastOfType(net::msg::AUTH).is_null(); });
            const std::string aliceId = alice.lastOfType(net::msg::AUTH)["playerId"];
            check(aliceId.size() == 10 && aliceId.rfind("P-", 0) == 0, "registering gives a player ID: " + aliceId);

            mallory.net.send({{"t", net::msg::REGISTER}, {"username", "ALICE"}, {"password", "other123"}});
            pump(loginServer, clients, [&] { return !mallory.lastOfType(net::msg::ERROR).is_null(); });
            check(mallory.lastOfType(net::msg::ERROR)["msg"] == "That username is already taken",
                  "usernames are unique (ignoring upper/lower case)");

            mallory.received.clear();
            mallory.net.send({{"t", net::msg::LOGIN}, {"username", "alice"}, {"password", "wrong-guess"}});
            pump(loginServer, clients, [&] { return !mallory.lastOfType(net::msg::ERROR).is_null(); });
            check(mallory.lastOfType(net::msg::ERROR)["msg"] == "Wrong username or password", "a wrong password is refused");

            bob.net.send({{"t", net::msg::LOGIN}, {"username", "alice"}, {"password", "secret123"}});
            pump(loginServer, clients, [&] { return !bob.lastOfType(net::msg::AUTH).is_null(); });
            check(bob.lastOfType(net::msg::AUTH)["playerId"] == aliceId, "logging in again gives the same player ID");

            // Alice hosts; her name comes from the account, not from what the client claims
            alice.received.clear();
            alice.net.send({{"t", net::msg::CREATE}, {"name", "TotallyNotAlice"}});
            pump(loginServer, clients, [&] { return !alice.lastOfType(net::msg::LOBBY).is_null(); });
            const json aliceLobby = alice.lastOfType(net::msg::LOBBY);
            check(aliceLobby["players"][0]["name"] == "Alice" && aliceLobby["players"][0]["playerId"] == aliceId,
                  "the lobby shows the account name and player ID");

            // The same account can't take a second seat in the same room
            bob.received.clear();
            bob.net.send({{"t", net::msg::JOIN}, {"code", alice.lastOfType(net::msg::JOINED)["code"]}});
            pump(loginServer, clients, [&] { return !bob.lastOfType(net::msg::ERROR).is_null(); });
            check(bob.lastOfType(net::msg::ERROR)["msg"] == "This account is already in the room",
                  "one account cannot sit in the same room twice");

            // Password guessing gets the connection closed
            for (int i = 0; i < net::MAX_FAILED_LOGINS; ++i)
                mallory.net.send({{"t", net::msg::LOGIN}, {"username", "alice"}, {"password", "guess" + std::to_string(i)}});
            pump(loginServer, clients, [&] { return mallory.net.status() == NetClient::Status::Disconnected; });
            check(true, "after " + std::to_string(net::MAX_FAILED_LOGINS) + " failed logins the connection is closed");
        }

        std::cout << "All network tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
