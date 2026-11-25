# Game data (SCRUM-5)

This folder contains the configuration data for Dungeon Party: cards, heroes, monsters,
match rules and levels. The game reads these files when it starts
(loader: `include/GameData.h`, `src/GameData.cpp`), so values can be changed here
**without changing C++ code**.

| File | Content | Used by |
|---|---|---|
| `cards.json` | All cards (hero cards and DM cards) | Card/deck model (SCRUM-15), HUD (SCRUM-11) |
| `heroes.json` | Hero classes, pictures and starting decks | Match setup (SCRUM-6), battle screen (SCRUM-9) |
| `monsters.json` | Monster kinds and pictures | Match setup (SCRUM-6), monster attack rules (SCRUM-20), battle screen (SCRUM-9) |
| `rules.json` | Match rules: cards per turn, drawing, shuffling, mana, turn timer | Turn manager (SCRUM-13), drawing (SCRUM-16), mana (SCRUM-17), timer (Sprint 2) |
| `levels/level_N.json` | One file per level: round limit, monsters per hero player | Match setup (SCRUM-6) |

**These files only hold values.** The loader checks that the values are well-formed, but it does
not apply any game rule. Shuffling, drawing, mana refill, the card limit and the timer are
implemented by the game logic in their own tasks.

Final game values are decided by **Irma**. Entries marked `"temporary": true` are not confirmed
as final yet – see the table at the end.

---

## General rules

- The files are plain JSON. JSON does not allow comments.
- Fields whose name starts with `_` (for example `"_note"`) are comments. The loader ignores them.
- Every `id` must be unique inside its file and should use lowercase letters, digits and `_`
  (for example `strike`, `goblin`, `fire_ball`). Other files refer to entries by this `id`.
- Numbers must be whole numbers (`3`, not `3.5` and not `"3"`).
- Every entry has `"temporary": true` or `"temporary": false`.
  `true` means the values are not confirmed as final yet. The data check prints a warning
  for every temporary entry.
- Enum values are written in CAPITAL letters, exactly like the enums in `include/Types.h`.
- Picture paths (`image`) are relative to the project folder and use `/` (never `\`).
  The pictures are in `assets/`. The data check warns (but does not fail) if a picture is missing.

---

## cards.json

```json
{
  "cards": [
    {
      "id": "strike",
      "name": "Strike",
      "side": "HEROES",
      "effect": "ATTACK",
      "manaCost": 7,
      "value": 10,
      "description": "Deal damage to one monster.",
      "temporary": true
    }
  ]
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `id` | text | unique, not empty | Name used by decks to refer to this card |
| `name` | text | not empty | Name shown in the game |
| `side` | text | `HEROES` or `DUNGEON_MASTER` | Who can use the card (`Side` in `Types.h`). `HEROES` → `HeroCard`, `DUNGEON_MASTER` → `DMCard` |
| `effect` | text | see below | What the card does |
| `manaCost` | whole number | 0 or more | Mana needed to play the card |
| `value` | whole number | 0 or more | Strength of the effect (damage for `ATTACK`/`SPELL`, healing for `HEAL`, …) |
| `description` | text | not empty | Short text for the HUD. Do not write numbers here – the HUD shows `manaCost` and `value` separately, so the text stays correct when values change |
| `temporary` | true/false | required | `true` = values not final yet |

Allowed `effect` values (from `Types.h`):

- `side: "HEROES"` → `ATTACK`, `HEAL`, `BUFF`, `SPELL` (`HeroEffect`)
- `side: "DUNGEON_MASTER"` → `SUMMON`, `TRAP`, `DEFENSIVE`, `ATTACK` (`DMEffect`)

Sprint 1 has exactly one card: the attack card `strike`.

---

## heroes.json

```json
{
  "heroes": [
    {
      "id": "warrior",
      "name": "Warrior",
      "heroClass": "WARRIOR",
      "maxHealth": 100,
      "maxMana": 50,
      "image": "assets/Warrior_Artwork.png",
      "startingDeck": [ { "card": "strike", "count": 5 } ],
      "temporary": true
    }
  ]
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `id` | text | unique, not empty | Name used by levels to refer to this hero |
| `name` | text | not empty | Name shown in the game |
| `heroClass` | text | `WARRIOR`, `RANGER`, `CLERIC` or `WIZARD` | Hero class (`HeroClass` in `Types.h`) |
| `maxHealth` | whole number | 1 or more | Health; the hero starts the match with full health (`HeroType::maxHealth`) |
| `maxMana` | whole number | 0 or more | Mana; the hero starts with full mana and mana is refilled every round (`Player::maxMana`) |
| `image` | text | not empty, `/` only | Picture of the hero, relative to the project folder |
| `startingDeck` | list | **may be empty** | Cards in the hero's deck at match start. An empty deck `[]` is a valid state |
| `startingDeck[].card` | text | must be a **HEROES** card in `cards.json` | Which card |
| `startingDeck[].count` | whole number | 1 or more | How many copies |
| `temporary` | true/false | required | `true` = values not final yet |

Playable heroes: **Warrior**, **Healer** (`CLERIC`) and **Wizard** (`WIZARD`, same cards as the Warrior).
`RANGER` is accepted as a class name but has no hero subclass yet.

> **CLERIC vs HEALER (known inconsistency):** the team uses `CLERIC` (Vision document,
> UML OCL constraints, UML persistence diagram, Sprint 1 requirements). Kagan's `Types.h`
> calls this class `HEALER`. Until the enum is renamed, the loader translates `"CLERIC"` to
> `HeroClass::HEALER`. See the `TODO` in `src/GameData.cpp`. Do **not** write `HEALER` in the
> data files.

---

## monsters.json

```json
{
  "monsters": [
    {
      "id": "goblin",
      "name": "Goblin",
      "maxHealth": 100,
      "attackPower": 15,
      "boss": false,
      "image": "assets/Monster_Artwork.png",
      "temporary": true
    }
  ]
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `id` | text | unique, not empty | Name used by levels to refer to this monster |
| `name` | text | not empty | Name shown in the game |
| `maxHealth` | whole number | 1 or more | Health at the start of the match |
| `attackPower` | whole number | 0 or more | Damage of the monster's attack (UML: `Monster.attackPower`) |
| `boss` | true/false | required | `true` = this monster is a final **boss monster** (UML: `Monster.boss`). Sprint 1 has no boss monster |
| `image` | text | not empty, `/` only | Picture of the monster, relative to the project folder |
| `temporary` | true/false | required | `true` = values not final yet |

Notes:

- `attackPower` is in the UML but not yet a member of the `Monster` class. It is stored here so the
  monster attack rules can use it.
- The UML `Monster.level` field is **not** in this file, because its meaning is still open
  (monster strength or dungeon level?). To be clarified with Kagan.

---

## rules.json

```json
{
  "maxCardsPlayedPerTurn": 3,
  "cardsDrawnPerTurn": 2,
  "shuffleDeckAtMatchStart": true,
  "refillManaEachRound": true,
  "maxManaIncreasePerRound": 0,
  "heroTurnTimeLimitSeconds": 60,
  "dmTurnTimeLimitSeconds": 60,
  "temporary": true
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `maxCardsPlayedPerTurn` | whole number | 1 or more | A player may play at most this many cards per turn |
| `cardsDrawnPerTurn` | whole number | 0 or more | Cards drawn at the start of each turn. Drawing from an empty deck is not an error |
| `shuffleDeckAtMatchStart` | true/false | required | Shuffle every deck once when the match starts |
| `refillManaEachRound` | true/false | required | Mana goes back to `maxMana` every round |
| `maxManaIncreasePerRound` | whole number | 0 or more | How much `maxMana` grows per round. `0` = it never grows (Sprint 1) |
| `heroTurnTimeLimitSeconds` | whole number | 1 or more | Time limit of a hero turn. When it runs out, the server ends the turn automatically |
| `dmTurnTimeLimitSeconds` | whole number | 1 or more | Time limit of the Dungeon Master's turn. Optional – if missing, the hero value is used |
| `temporary` | true/false | required | `true` = values not final yet |

---

## levels/level_N.json

One file per level. The file name number and `number` inside the file must match
(`level_1.json` → `"number": 1`). New level files appear in the level selection automatically.

```json
{
  "number": 2,
  "name": "Level 2: The Dragon's Lair",
  "maxRounds": 14,
  "background": "assets/Level_2_Artwork.png",
  "defaultHeroes": ["warrior"],
  "swarms": [
    { "name": "Crypt guards",
      "monsters": [ { "monster": "skeleton", "countPerHero": 1 }, { "monster": "goblin", "count": 1 } ] },
    { "name": "The Lich's court",
      "monsters": [ { "monster": "lich", "count": 1 }, { "monster": "skeleton", "countPerHero": 1 } ] }
  ],
  "boss": { "monster": "dragon" },
  "dungeonMaster": { "maxMana": 12, "startingDeck": [ { "card": "goblin_slash", "count": 5 } ] },
  "temporary": true
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `number` | whole number | 1 or more, same as file name | Level number |
| `name` | text | not empty | Name shown in the game |
| `maxRounds` | whole number | 1 or more | Round limit: the DM side wins when it is reached (`Match::maxRounds`) |
| `background` | text (path) | optional, use `/` | Battle background for this level. Missing = `assets/Level_1_Artwork.png` |
| `defaultHeroes` | list of hero ids | at least 1, each must exist in `heroes.json` | Hero for players who don't pick a role |
| `swarms` | list | at least 1 swarm | The monster waves, in order (see below) |
| `swarms[].name` | text | optional | Shown in the battle log ("Swarm 2 of 2 appears: The Lich's court") |
| `swarms[].monsters` | list | at least 1 entry | The monsters of this swarm; **different kinds can be mixed** |
| `...monsters[].monster` | text | must exist in `monsters.json`, not a boss | Which monster |
| `...monsters[].countPerHero` | whole number | 0 or more | How many appear **per hero player** |
| `...monsters[].count` | whole number | 0 or more | How many appear **in addition**, independent of the number of heroes |
| `boss` | object | optional, `monster` must be a boss | The form the Dungeon Master takes when he enters the battle |
| `boss.arrivalRound` | whole number | optional | Only for older levels: boss arrives in this round instead of after the swarms |
| `dungeonMaster` | object | optional | The DM's mana and starting deck (DUNGEON_MASTER cards only) |
| `temporary` | true/false | required | `true` = values not final yet |

`countPerHero` + `count` must be at least 1 for every monster line.

**How swarms work:** the match starts with swarm 1. When every monster on the battlefield is
defeated, the next swarm appears. After the last swarm, the Dungeon Master enters as the `boss`
monster. The heroes win only when the boss is defeated too. Without `boss.arrivalRound` the boss
never arrives after a fixed number of rounds.

Older files with a single `"monsters": [...]` list instead of `"swarms"` still work: that list is one swarm.


The match setup calculates each monster count as `countPerHero × number of hero players + count`.

---

## Naming notes

- **"Boss" vs `boss`:** in team discussions "Boss" sometimes means the **Dungeon Master role**.
  The `boss` field in `monsters.json` is something else: the UML `Monster.boss` flag for a final
  boss **monster**. These are not the same concept. The data files contain nothing about the
  Dungeon Master role yet.
- **"Game data" vs `GameData`:** in team discussions "game data" sometimes means the game loop and
  the state of a running match. The `GameData` class only **loads and checks the configuration
  files** in this folder. The game loop and match state belong to the game logic (`Match`,
  turn manager).

---

## Temporary values

All values below come from the Sprint 1 requirements (team chat, 23.10.2026). They stay marked
`"temporary": true` until they are confirmed as final.

| File | Entry | Field | Value |
|---|---|---|---|
| `cards.json` | `strike` | `effect` / `value` / `manaCost` | `ATTACK` / 10 / 7 |
| `heroes.json` | `warrior` | `maxHealth` / `maxMana` | 100 / 50 |
| `heroes.json` | `warrior` | Strike cards in `startingDeck` | 5 |
| `heroes.json` | `warrior` | `image` | `assets/Warrior_Artwork.png` |
| `monsters.json` | `goblin` | `maxHealth` / `attackPower` / `boss` | 100 / 15 / false |
| `monsters.json` | `goblin` | `image` | `assets/Monster_Artwork.png` |
| `rules.json` | – | cards per turn / drawn per turn / shuffle | 3 / 2 / yes |
| `rules.json` | – | mana refill / max mana increase / turn timer | every round / 0 / 30 s |
| `levels/level_1.json` | – | `maxRounds` | 3 |
| `levels/level_1.json` | – | goblins per hero player | 1 |

When a value is confirmed: change it in the JSON file if needed, set `"temporary": false` for that
entry once **all** its values are final, and update this table.

---

## How to add new data

- **New card:** add an entry to `cards.json`. No C++ change is needed if the effect already exists.
- **New hero class:** add an entry to `heroes.json` (`WARRIOR`, `RANGER`, `CLERIC`, `WIZARD`; a new class also needs a `Hero` subclass).
- **New monster kind:** add an entry to `monsters.json`.
- **New level:** copy `levels/level_1.json` to `levels/level_2.json` and change `number` to `2`.

## Checking the data

Run the data check after every change. From the project folder:

```
cmake -S . -B build
cmake --build build --target validate_data
./build/validate_data
```

On Windows with Visual Studio the program is `build\Debug\validate_data.exe`.
`ctest --test-dir build` runs the same check as a test. Exit code 0 = data OK
(temporary values and missing pictures are only warnings), exit code 1 = error
(the message names the file and the entry).

## Card validation regression tests (SCRUM-48)

Build `card_data_tests` and run `ctest --test-dir <build-folder> -R "^card_data_" --output-on-failure`.
The tests use temporary copies, leaving the real data files unchanged. They cover valid
hero/DM effects, missing or malformed files, invalid fields and numbers, duplicate IDs,
unknown effects, and invalid hero deck references. Loading a valid effect does not mean
its gameplay behaviour is implemented.


## SCRUM-62/64: supplied level and monster draft

These entries follow the team document *New levels designs and monster stats*.
All supplied values remain `temporary: true`.

| Level | Monsters per hero | Boss | Arrival round | DM mana | DM deck |
|---|---|---|---|---|---|
| 1 (existing) | 1 Goblin | Final Boss | 4 | 10 | 12 Monster Attack |
| 2: The Crypt | 2 Skeletons | Lich | 5 | 12 | 12 Monster Attack |
| 3: The Dragon's Lair | 1 Goblin + 1 Orc | Dragon | 6 | 15 | 12 Monster Attack |

The new level JSON files use the document's `monsters` lists and fixed
`boss.arrivalRound`, supported by the existing loader. They do not add extra
swarms. The generic swarm engine remains available for other level files.

`monster_attack` is restored from the project's existing definition in commit
`6eeec1d`: mana cost 3 and damage 12. The supplied document refers to that card;
no new card cost was chosen for this update.

As the document states, `maxRounds: 30` in levels 2 and 3 is a placeholder.
The engine still enforces it as a real round limit. Level 1 retains its existing
round-limit field because the document does not supply a replacement.
Regular monsters still deal the DM attack card's damage; their individual
`attackPower` values are stored as specified but are not used by the current
attack code. Boss attacks use their configured attack power.

The three supplied Skeleton, Lich and Dragon images are also available under
the filenames used in the document. `assets/Orc_Artwork.png` is still missing;
no replacement monster image has been assigned. New levels use the existing
default background because the document does not specify backgrounds.
