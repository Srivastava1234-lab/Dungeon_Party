#include "TimedEffects.h"
#include <algorithm>
#include <stdexcept>

void TimedEffects::add(TimedEffectType type, int value, int rounds) {
    if (rounds <= 0) {
        throw std::invalid_argument("A timed effect needs at least 1 round");
    }

    // Same type already active -> restart duration, keep higher value
    for (TimedEffect& effect : effects) {
        if (effect.type == type) {
            effect.roundsLeft = rounds;
            effect.value = std::max(effect.value, value);
            return;
        }
    }

    effects.push_back(TimedEffect{type, value, rounds});
}

int TimedEffects::valueOf(TimedEffectType type) const {
    for (const TimedEffect& effect : effects) {
        if (effect.type == type) return effect.value;
    }
    return 0;
}

int TimedEffects::roundsLeft(TimedEffectType type) const {
    for (const TimedEffect& effect : effects) {
        if (effect.type == type) return effect.roundsLeft;
    }
    return 0;
}

void TimedEffects::tick() {
    for (TimedEffect& effect : effects) {
        effect.roundsLeft--;
    }

    // Remove every effect that reached 0.
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [](const TimedEffect& e) { return e.roundsLeft <= 0; }),
        effects.end());
}

const std::vector<TimedEffect>& TimedEffects::all() const {
    return effects;
}
