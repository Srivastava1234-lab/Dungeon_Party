# Game data (SCRUM-5)

This folder contains the game data for Dungeon Party: cards, heroes, monsters and levels.
The game reads these files when it starts (loader: `include/GameData.h`, `src/GameData.cpp`),
so values can be changed here **without changing C++ code**.

| File | Content | Used by |
|---|---|---|
| `cards.json` | All cards (hero cards and DM cards) | Card/deck model (Taegan), HUD (Soroush) |
| `heroes.json` | Hero classes and their starting decks | Match setup (Kagan) |
| `monsters.json` | Monster kinds (and later the boss) | Match setup (Kagan), monster attack rules |
| `levels/level_N.json` | One file per level: monsters, round limit | Match setup (Kagan) |

Final game values (health, damage, mana costs, round limit) are decided by **Irma**.
Most values in this folder are still **temporary** – see the table at the end.

---

## General rules

- The files are plain JSON. JSON does not allow comments.
- Fields whose name starts with `_` (for example `"_note"`) are comments. The loader ignores them.
- Every `id` must be unique inside its file and should use lowercase letters, digits and `_`
  (for example `strike`, `goblin`, `fire_ball`). Other files refer to entries by this `id`.
- Numbers must be whole numbers (`3`, not `3.5` and not `"3"`).
- Every entry has `"temporary": true` or `"temporary": false`.
  `true` means the numbers are placeholders and must still be confirmed by Irma.
  The data check prints a warning for every temporary entry.
- Enum values are written in CAPITAL letters, exactly like the enums in `include/Types.h`.

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
      "manaCost": 1,
      "value": 6,
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

Sprint 1 only uses `ATTACK`.

---

## heroes.json

```json
{
  "heroes": [
    {
      "id": "warrior",
      "name": "Warrior",
      "heroClass": "WARRIOR",
      "maxHealth": 30,
      "maxMana": 3,
      "startingDeck": [ { "card": "strike", "count": 10 } ],
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
| `maxHealth` | whole number | 1 or more | Health at the start of the match (`HeroType::maxHealth`) |
| `maxMana` | whole number | 0 or more | Mana refilled every round (`Player::maxMana`) |
| `startingDeck` | list | at least 1 entry | Cards in the hero's deck at match start |
| `startingDeck[].card` | text | must be a **HEROES** card in `cards.json` | Which card |
| `startingDeck[].count` | whole number | 1 or more | How many copies |
| `temporary` | true/false | required | `true` = values not final yet |

> **CLERIC vs HEALER (known inconsistency):** the team agreed to use `CLERIC` (Vision document,
> UML OCL constraints, UML persistence diagram). `Types.h` still calls this class `HEALER`.
> Until the enum is renamed, the loader translates `"CLERIC"` to `HeroClass::HEALER`.
> See the `TODO` in `src/GameData.cpp`. Do **not** write `HEALER` in the data files.

---

## monsters.json

```json
{
  "monsters": [
    {
      "id": "goblin",
      "name": "Goblin",
      "maxHealth": 20,
      "attackPower": 1,
      "boss": false,
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
| `boss` | true/false | required | `true` = this monster is a final boss (UML: `Monster.boss`) |
| `temporary` | true/false | required | `true` = values not final yet |

Notes:

- `attackPower` is in the UML but not yet a member of the `Monster` class. It is stored here so the
  monster attack rules can use it.
- The UML `Monster.level` field is **not** in this file yet, because its meaning is still open
  (monster strength or dungeon level?). To be clarified with Kagan.
- Boss vs. Monster naming is still open in the team. The `boss` flag works either way.

---

## levels/level_N.json

One file per level. The file name number and `number` inside the file must match
(`level_1.json` → `"number": 1`).

```json
{
  "number": 1,
  "name": "Level 1",
  "maxRounds": 20,
  "defaultHeroes": ["warrior"],
  "monsters": [ { "monster": "goblin", "count": 1 } ],
  "temporary": true
}
```

| Field | Type | Rule | Meaning |
|---|---|---|---|
| `number` | whole number | 1 or more, same as file name | Level number |
| `name` | text | not empty | Name shown in the game |
| `maxRounds` | whole number | 1 or more | Round limit: the DM side wins when it is reached (`Match::maxRounds`) |
| `defaultHeroes` | list of hero ids | at least 1, each must exist in `heroes.json` | Heroes used when there is no role selection yet (Sprint 1 only) |
| `monsters` | list | at least 1 entry | Monsters that appear in this level |
| `monsters[].monster` | text | must exist in `monsters.json` | Which monster |
| `monsters[].count` | whole number | 1 or more | How many of this monster |
| `temporary` | true/false | required | `true` = values not final yet |

---

## Temporary values (to be confirmed by Irma)

| File | Entry | Field | Current value | Where it comes from |
|---|---|---|---|---|
| `cards.json` | `strike` | `manaCost` | 1 | Kagan's `console_main.cpp` |
| `cards.json` | `strike` | `value` (damage) | 6 | Kagan's `console_main.cpp` |
| `heroes.json` | `warrior` | `maxHealth` | 30 | Kagan's `console_main.cpp` |
| `heroes.json` | `warrior` | `maxMana` | 3 | Kagan's `console_main.cpp` |
| `heroes.json` | `warrior` | Strike cards in deck | 10 | Kagan's `console_main.cpp` |
| `monsters.json` | `goblin` | `maxHealth` | 20 | Kagan's `console_main.cpp` |
| `monsters.json` | `goblin` | `attackPower` | 1 | **Placeholder only** – no value exists yet. 1 is the smallest value that still lets us see monster attacks working |
| `levels/level_1.json` | – | `maxRounds` | 20 | Default in Kagan's `Match.h` |
| `levels/level_1.json` | – | number of goblins | 1 | Kagan's `console_main.cpp` |

When Irma confirms a value: change it in the JSON file, set `"temporary": false` for that entry
once **all** its values are final, and update this table.

---

## How to add new data

- **New card:** add an entry to `cards.json`. No C++ change is needed if the effect already exists.
- **New hero class:** add an entry to `heroes.json` (only `WARRIOR`, `RANGER`, `CLERIC` exist).
- **New monster kind:** add an entry to `monsters.json`.
- **New level:** copy `levels/level_1.json` to `levels/level_2.json` and change `number` to `2`.

After every change, run the data check (`validate_data`, see the main README / SCRUM-5 pull request).
