# Online play - protocol and setup (Sprint 2 prototype)

## How it fits together

```
 dungeon_online (Akshat) ─┐
 dungeon_online (Kagan)  ─┼──UDP 7777──►  dungeon_server  ──► Room "K7RM2Q" → Match
 dungeon_online (Hülya)  ─┘                               └─► Room "P3XW9A" → Match
```

- One server hosts many rooms. Each room has a 6-character code and its own `Match`.
- Messages from a room only go to that room's members. A player can only act in the room they joined.
- The server is authoritative: clients send requests, the server checks them with `Match::validate/apply`
  and sends everyone the new state. Each player only receives their own hand.
- Codes use `ABCDEFGHJKMNPQRSTUVWXYZ23456789` (no 0/O/1/I/L). Typing is case-insensitive; spaces and dashes are ignored.
- 5 wrong codes and the server disconnects you, so codes can't be guessed.

## Messages

Every message is one JSON object in one reliable ENet packet. `"t"` is the type. Names are in `net/Protocol.h`.

### Client → server

| t        | fields              | notes                                       |
|----------|---------------------|---------------------------------------------|
| `create` | `name`              | make a room; you become host                |
| `join`   | `name`, `code`      | join a room that hasn't started yet         |
| `role`   | `role`              | lobby: `"dm"` or a hero id (`"warrior"`, `"healer"`); only one DM per room |
| `start`  | -                   | host only                                   |
| `play`   | `card`, `target`, `targetType` | hero: hand index, then `"monster"` + index into `monsters` or `"hero"` + index into `heroes` (Heal) |
| `command`| `card`, `monster`, `hero` | DM: order monster #m to attack hero #h with DM card #c; happens on End turn |
| `boss`   | `hero`              | DM: order the final boss to attack hero #h (once it has arrived) |
| `end`    | -                   | end your turn                               |
| `leave`  | -                   | leave the room (closing the connection also works) |

### Server → client

| t        | fields                                    | notes |
|----------|-------------------------------------------|-------|
| `joined` | `code`, `you`                             | you are in the room; `you` is your player id (`p1`, `p2`, ...) |
| `lobby`  | `code`, `host`, `players[{id,name,role}]` | sent to the whole room whenever it changes |
| `state`  | see below                                 | sent to each player after every change |
| `error`  | `msg`                                     | your request was rejected (wrong turn, not enough mana, ...) |
| `closed` | `msg`                                     | room closed |

`state`: `code, you, host, role ("hero" | "dm"), round, phase, active (player id), winner (null | "HEROES" | "DUNGEON_MASTER"),
heroes[{id, name, heroId, hp, maxHp, mana, maxMana, handSize, connected}], monsters[{name, hp, maxHp, alive, boss}],
dm{id, name, human, connected, mana*, maxMana*}, hand[{name, cost, effect, value}]` (only yours; the DM's on their turn),
`cardsPlayed`, `canBossAttack*`. Fields marked * are only sent to the DM.

## Rules

- Players pick a role in the lobby. No role = the level's default hero (`defaultHeroes` in the level file).
- Heroes come from `heroes.json`, the DM's mana and deck from the `dungeonMaster` block in the level file.
- No human DM = the computer plays the DM: each living monster attacks the weakest hero, and the boss attacks too.
- The final boss from the level file arrives in its `arrivalRound`; its health is `healthPerHero` x number of heroes.
- If the host leaves the lobby, the next player becomes host. If someone leaves during the match, their hero
  stays and their turns are skipped. Rejoining is not supported yet.
- The room closes when its last player leaves.

## Hosting: this computer vs. a server

- **This computer**: the game runs a RoomServer inside itself. Only players on the same network can reach it,
  unless the router forwards UDP 7777 to this computer.
- **A server**: the game connects to a `dungeon_server` somewhere else (e.g. the team laptop). This is the way to
  play across networks - that machine must be reachable (port forwarding or Tailscale).

## Running it

Local test (one PC): start `dungeon_server`, then two `dungeon_online` windows with server `127.0.0.1`.

Different networks: run the server on a machine everyone can reach, usually a small Linux cloud VM.

```
cmake -S . -B build && cmake --build build --target dungeon_server
./build/dungeon_server 7777 /path/to/Dungeon_Party/data
```

- Open **UDP** port 7777 in the VM's firewall / network security group. It's UDP, not TCP; this is the usual mistake.
- Players enter the VM's public IP (or a domain pointing at it) in the "Server" field.
- To keep it running after you log out, run it as a systemd service or inside `tmux`.

## Known gaps

- No encryption or accounts. Fine for a course project; don't reuse the protocol for anything sensitive.
- No reconnect after a dropped connection.
- No version check between client and server builds; everyone should run the same commit.
