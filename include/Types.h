#ifndef DUNGEON_PARTY_TYPES_H
#define DUNGEON_PARTY_TYPES_H

#include <optional>
#include <string>


enum class HeroEffect { ATTACK, HEAL, BUFF, SPELL };
enum class DMEffect { SUMMON, TRAP, DEFENSIVE, ATTACK};
enum class Phase { ROUND_START, HERO_TURN, DM_TURN, MONSTER_ACTIONS };
enum class Side { HEROES, DUNGEON_MASTER };
enum class HeroClass { WARRIOR, RANGER, HEALER };
enum class AttackCategory { HEROES, DUNGEON_MASTER };
struct CheckResult { bool valid; std::optional<std::string> reason; };
struct HeroType { HeroClass heroClass; int maxHealth; };

#endif