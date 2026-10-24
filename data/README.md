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
- `side: "DUNGEON_MASTER"` → `SUMMON`, `TRAP`, `DEFENSIVE` (`DMEffect`)

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
| `heroClass` | text | `WARRIOR`, `RANGER` or `CLERIC` | Hero class (`HeroClass` in `Types.h`) |
| `maxHealth` | whole number | 1 or more | Health; the hero starts the match with full health (`HeroType::maxHealth`) |
| `maxMana` | whole number | 0 or more | Mana; the hero starts with full mana and mana is refilled every round (`Player::maxMana`) |
| `image` | text | not empty, `/` only | Picture of the hero, relative to the project folder |
| `startingDeck` | list | **may be empty** | Cards in the hero's deck at match start. An empty deck `[]` is a valid state |
| `startingDeck[].card` | text | must be a **HEROES** card in `cards.json` | Which card |
| `startingDeck[].count` | whole number | 1 or more | How many copies |
| `temporary` | true/false | required | `true` = values not final yet |

Sprint 1: **WARRIOR is the only playable hero**, so `heroes.json` only contains the warrior.
`RANGER` and `CLERIC` are accepted class names for later sprints.

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
  "heroTurnTimeLimitSeconds": 30,
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
| `heroTurnTimeLimitSeconds` | whole number | 1 or more | Time limit of a hero turn. **Config only** – the timer is implemented in Sprint 2 |
| `temporary` | true/false | required | `true` = values not final yet |

---

## levels/level_N.json

One file per level. The file name number and `number` inside the file must match
(`level_1.json` → `"number": 1`).

```json
{
  "number": 1,
  "name": "Level 1",
  "maxRounds": 3,
  "defaultHeroes": ["warrior"],
  "monsters": [ { "monster": "goblin", "countPerHero": 1 } ],
  "temporary": true
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `number` | whole number | 1 or more, same as file name | Level number |
| `name` | text | not empty | Name shown in the game |
| `maxRounds` | whole number | 1 or more | Round limit: the DM side wins when it is reached (`Match::maxRounds`) |
| `defaultHeroes` | list of hero ids | at least 1, each must exist in `heroes.json` | Heroes used when there is no role selection yet (Sprint 1 only) |
| `monsters` | list | at least 1 entry | Monster kinds that appear in this level |
| `monsters[].monster` | text | must exist in `monsters.json` | Which monster |
| `monsters[].countPerHero` | whole number | 1 or more | How many of this monster appear **per hero player** |
| `temporary` | true/false | required | `true` = values not final yet |

A level stores **no fixed total number of monsters** and **no grid, tiles or positions**.
The match setup works out the number of monsters as `countPerHero × number of hero players`.

`assets/Level_1_Artwork.png` exists, but a level has no picture field yet, because the Sprint 1
requirements only ask for hero and monster pictures.

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
- **New hero class:** add an entry to `heroes.json` (only `WARRIOR`, `RANGER`, `CLERIC` exist).
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
