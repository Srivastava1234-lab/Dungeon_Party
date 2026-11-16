# Online play - protocol and setup

## How it fits together

```
 Dungeon_Party (Akshat) ─┐
 Dungeon_Party (Kagan)  ─┼──UDP 7777──►  dungeon_server  ──► Room "K7RM2Q" → Match
 Dungeon_Party (Hülya)  ─┘                               └─► Room "P3XW9A" → Match
```

- One server hosts many rooms. Each room has a 6-character code and its own `Match`.
- Messages from a room only go to that room's members. A player can only act in the room they joined.
- The server is authoritative: clients send requests, the server checks them with `Match::validate/apply`
  and sends everyone the new state. Each player only receives their own hand.
- Codes use `ABCDEFGHJKMNPQRSTUVWXYZ23456789` (no 0/O/1/I/L). Typing is case-insensitive; spaces and dashes are ignored.
- 5 wrong codes or rejoin attempts and the server disconnects you, so codes/tokens can't be guessed.
- Connecting clients pass `net::PROTOCOL_VERSION` (`2`) in the ENet handshake; mismatched versions are rejected cleanly.

## Messages

Every message is one JSON object in one reliable ENet packet. `"t"` is the type. Names are in `net/Protocol.h`.

### Client → server

| t        | fields              | notes                                       |
|----------|---------------------|---------------------------------------------|
| `create` | `name`              | make a room; you become host                |
| `join`   | `name`, `code`      | join a room that hasn't started yet         |
| `rejoin` | `code`, `token`     | reclaim a disconnected seat in an active match |
| `role`   | `role`              | lobby: `"dm"` or a hero id (`"warrior"`, `"healer"`); only one DM per room |
| `start`  | -                   | host only                                   |
| `play`   | `card`, `target`, `targetType` | hero: hand index, then `"monster"` + index into `monsters` or `"hero"` + index into `heroes` (Heal) |
| `command`| `card`, `monster`, `hero` | DM: order monster #m to attack hero #h with DM card #c; happens on End turn |
| `boss`   | `hero`              | DM: order the final boss to attack hero #h (once it has arrived) |
| `end`    | -                   | end your turn                               |
| `leave`  | -                   | explicitly leave the room (forfeits reconnect token during a match) |

### Server → client

| t        | fields                                    | notes |
|----------|-------------------------------------------|-------|
| `joined` | `code`, `you`, `token`, (`rejoined`)      | you are in the room; `you` is your player id (`p1`, `p2`, ...); `token` allows reconnecting |
| `lobby`  | `code`, `host`, `players[{id,name,role}]` | sent to the whole room whenever it changes |
| `state`  | see below                                 | sent to each player after every change |
| `error`  | `msg`                                     | your request was rejected (wrong turn, not enough mana, ...) |
| `closed` | `msg`                                     | room closed |

`state`: `code, you, host, role ("hero" | "dm"), round, phase, active (player id), winner (null | "HEROES" | "DUNGEON_MASTER"),
heroes[{id, name, heroId, hp, maxHp, mana, maxMana, handSize, connected}], monsters[{name, hp, maxHp, alive, boss}],
dm{id, name, human, connected, mana*, maxMana*, deckSize*, queued*[{monster, hero, card}]}, hand[{name, cost, effect, value}]` (only yours; the DM's on their turn),
`cardsPlayed`, `canBossAttack*`. Fields marked * are only sent to the DM.

## Rules & Reconnect

- Players pick a role in the lobby. No role = the level's default hero (`defaultHeroes` in the level file).
- Heroes come from `heroes.json`, the DM's mana and deck from the `dungeonMaster` block in the level file.
- The DM's orders this turn are kept on the server (`dm.queued`) and carried out when the DM ends the turn (SCRUM-40).
- No human DM = the computer plays the DM: each living monster attacks the weakest hero, and the boss attacks too.
- The final boss from the level file arrives in its `arrivalRound`; its health is `healthPerHero` x number of heroes.
- If the host leaves or drops, the next connected player becomes host. If someone drops during the match, their hero
  stays, their turns are skipped, and they can reconnect using `rejoin` (`code` + `token`).
- If every player drops mid-match, the room pauses and stays open for `EMPTY_ROOM_GRACE_SECONDS` (60s) so everyone can reconnect.
- Clicking **Leave** sends `leave` and clears the seat's reconnect token. Lobby rooms close immediately when empty.

## Hosting: this computer vs. a server

- **This computer**: the game runs a `RoomServer` inside itself. Players on the same network can reach it; players on
  different networks can only reach it if your router forwards UDP 7777 or via Tailscale / a UDP tunnel.
- **A server**: connect to a `dungeon_server` somewhere reachable (e.g. a free-tier Linux cloud VM, or a PC exposed via
  a free `playit.gg` UDP tunnel or Tailscale). Clients accept `host[:port]` (e.g. `myserver.ply.gg:12345`), and you can
  pre-fill a default server via `DUNGEON_PARTY_SERVER` env var or `-DDUNGEON_PARTY_DEFAULT_SERVER=...` in CMake.

## Running `dungeon_server`

Local test (one PC): start `dungeon_server`, then two `Dungeon_Party` windows with server `127.0.0.1`.

Different networks (Linux cloud VM):

```
cmake -S . -B build && cmake --build build --target dungeon_server
./build/dungeon_server 7777 /path/to/Dungeon_Party/data
```

- Open **UDP** port 7777 in the VM's firewall / network security group. It's UDP, not TCP; this is the usual mistake.
- Players enter the VM's public IP (or `host:port`) in the "Server" field.
- To keep it running after you log out, run it as a systemd service or inside `tmux`.

## Known gaps

- No encryption or user accounts. Fine for a course project; don't reuse the protocol for anything sensitive.
- Save/load state across server restarts is not implemented yet.
