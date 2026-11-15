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
        check(dmView["hand"].size() >= 1 && dmView["hand"][0]["effect"] == "DM_ATTACK", "DM sees their attack cards on their turn");

        // A hero cannot give monster orders
        heroPlayer.received.clear();
        heroPlayer.net.send({{"t", net::msg::COMMAND}, {"card", 0}, {"monster", 0}, {"hero", 0}});
        pump(server, roles, [&] { return !heroPlayer.lastOfType(net::msg::ERROR).is_null(); });
        check(true, "heroes cannot command monsters");

        // DM orders monster 0 to attack hero 0, then ends the turn: the hero takes damage
        const int healerHpBefore = dmView["heroes"][0]["hp"];
        dmPlayer.received.clear();
        dmPlayer.net.send({{"t", net::msg::COMMAND}, {"card", 0}, {"monster", 0}, {"hero", 0}});
        pump(server, roles, [&] { return !dmPlayer.lastOfType(net::msg::STATE).is_null() || !dmPlayer.lastOfType(net::msg::ERROR).is_null(); });
        check(dmPlayer.lastOfType(net::msg::ERROR).is_null(), "DM's monster order is accepted");
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

        std::cout << "All network tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
