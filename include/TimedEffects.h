#pragma once
#include "Types.h"
#include <vector>

class TimedEffects {
    std::vector<TimedEffect> effects;
public:
    void add(TimedEffectType type, int value, int turns);
    int valueOf(TimedEffectType type) const;
    int roundsLeft(TimedEffectType type) const;
    void tick();
    void clear();
    const std::vector<TimedEffect>& all() const { return effects; }
};
