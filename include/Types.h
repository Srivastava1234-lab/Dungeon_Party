#ifndef DUNGEON_PARTY_TYPES_H
#define DUNGEON_PARTY_TYPES_H

#include <optional>
#include <string>


enum class SpellTarget { ONE_ENEMY, ALL_ENEMIES, BOSS_ONLY };

enum class HeroEffect { ATTACK, HEAL, BUFF, SPELL, SHIELD, FIREBALL};
enum class DMEffect { SUMMON, TRAP, DEFENSIVE, ATTACK, WEAKNESS, MANA_TRAP, DISCARD_TRAP, MANA_DRAIN_TRAP};
enum class Phase { ROUND_START, HERO_TURN, DM_TURN, MONSTER_ACTIONS };
enum class Side { HEROES, DUNGEON_MASTER };
enum class HeroClass { WARRIOR, RANGER, HEALER, WIZARD };
enum class AttackCategory { HEROES, DUNGEON_MASTER };
struct CheckResult { bool valid; std::optional<std::string> reason; };
struct HeroType { HeroClass heroClass; int maxHealth; };
enum class SupportTarget {SELF_ONLY, SELF_OR_ALLY};
enum class TrapEffect {DAMAGE, DISCARD, MANA_DRAIN};
// SCRUM-55: effects that last a number of rounds
enum class TimedEffectType { BUFF };

struct TimedEffect {
    TimedEffectType type;
    int value;       // strength of the effect, e.g. the buff bonus
    int roundsLeft;  // counts down once per round; removed at 0
};

#endif