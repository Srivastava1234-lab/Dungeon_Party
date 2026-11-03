# Sprint 2 testing status - Hulya

All work stays on `hulya-Sprint2`. Local base: main `4bcbf34`.
This is a local test and coordination record, not a Jira acceptance or completion claim.

## Commands

```text
cmake -S . -B build
cmake --build build --target validate_data card_data_tests combat_tests monster_card_tests healing_tests
ctest --test-dir build --output-on-failure
```

The tests use explicit checks, so Release builds do not disable the assertions.
The graphical game was not rebuilt or manually tested for these changes.
The older model_tests, drawing_mana_tests and turn_indicator_tests are still not
registered here; the result below is for the 33 registered tests only.

## Latest local result

All 33 registered CTest cases passed against main `4bcbf34`, including all
seven healing tests. Five of these previously reproduced the healing regression
against `761618f`. The updated game code now adds the requested amount and caps
healing at maximum HP. No rule about revival of defeated heroes was introduced.
Tests use the current HEALER enum.

## SCRUM-48: card file validation

Local implementation and tests prepared: DM ATTACK loading and tool output, missing
files, JSON syntax/structure, required fields/types, numeric bounds, IDs, role/effect
compatibility and hero deck references. Ten new test groups plus the original data
validation pass. The validate_data CLI was also checked with a temporary DM ATTACK
card. Real production JSON values were not changed.

## SCRUM-46: monster card coverage

Ten passing groups cover deferred damage and one-time payment/discard, selected
execution order, insufficient mana, turn authority, foreign/dead targets, foreign or
unsupported cards, three-card limit, queue clearing, defeated participants and match
termination. These test existing DM ATTACK cards; SUMMON/TRAP/DEFENSIVE gameplay is
not claimed as covered or implemented. Confirm updated monster card rules with Kagan.

All seven healing tests pass with the fix in main.
Boss victory tests remain pending agreement on the final boss rule with Irma and
alignment with Akshat's boss attack rules. SCRUM-46 is therefore incomplete.

## SCRUM-44 and SCRUM-47: decisions needed

- Irma: boss stats, spell list, effects, mana costs, target restrictions and values.
- Irma: does defeating the boss end the match with ordinary monsters still alive?
- Irma/Akshat: when does the boss appear, and how do hero defeat/round-limit rules interact?
- Taegan/Kagan: confirm any changes to the card data/model interface.
- When drafting unapproved values, retain temporary: true; do not invent final rules.

No new boss or spell entries have been added.

## SCRUM-31: multi-computer test checklist (not executed)

Await a shared runnable network build and launch instructions from Akshat/Taegan,
plus hidden-information filtering from Soroush. The separate net-prototype branch
has not been incorporated into this local branch.

| Scenario | Check once networking is ready |
|---|---|
| Join from different computers | Clients join the same server/match; confirm identities and roles |
| Start prerequisites | Coordinate >=3 heroes and exactly one DM with the team's start-check implementation |
| State synchronization | After each accepted action compare round, active player, HP, mana and public board state on all clients |
| Private hands | Check the agreed role-specific visibility in server messages as well as the UI; another player's secret cards must not be sent to an unauthorized client |
| Unauthorized actions | Out-of-turn or invalid actions do not change authoritative state or charge resources |
| Network disruption | Record behaviour on disconnect/reconnect and agree expected handling with the network owners |

Record build SHA, machine/role mapping, steps, expected/actual results and evidence
for each real run. None of these multi-computer checks has been performed yet.
