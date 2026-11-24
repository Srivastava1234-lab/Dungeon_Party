#ifndef DUNGEON_PARTY_TIMED_EFFECTS_H
#define DUNGEON_PARTY_TIMED_EFFECTS_H

#include <vector>
#include "Types.h"

// SCRUM-55: the list of active timed effects on one character.
class TimedEffects {
    std::vector<TimedEffect> effects;

public:
    void add(TimedEffectType type, int value, int rounds);
    int valueOf(TimedEffectType type) const;    // 0 if not active
    int roundsLeft(TimedEffectType type) const; // 0 if not active
    void tick();                                // one round passed
    const std::vector<TimedEffect>& all() const;
};

#endif
