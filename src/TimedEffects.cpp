#include "TimedEffects.h"
#include <algorithm>
#include <stdexcept>

void TimedEffects::add(TimedEffectType type, int value, int turns) {
    if (turns <= 0 || value < 0) throw std::invalid_argument("Invalid timed effect");
    for (auto& effect : effects) {
        if (effect.type == type) {
            effect.value = std::max(effect.value, value);
            effect.roundsLeft = turns;
            return;
        }
    }
    effects.push_back({type, value, turns});
}
int TimedEffects::valueOf(TimedEffectType type) const {
    for (const auto& effect : effects) if (effect.type == type) return effect.value;
    return 0;
}
int TimedEffects::roundsLeft(TimedEffectType type) const {
    for (const auto& effect : effects) if (effect.type == type) return effect.roundsLeft;
    return 0;
}
void TimedEffects::tick() {
    for (auto& effect : effects) --effect.roundsLeft;
    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](const TimedEffect& effect) { return effect.roundsLeft <= 0; }), effects.end());
}
void TimedEffects::clear() { effects.clear(); }
