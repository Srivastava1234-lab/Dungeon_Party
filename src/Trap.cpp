#include "Trap.h"
#include <stdexcept>
Trap::Trap(int n) : damage(n) { 
    if (n < 0){
        throw std::invalid_argument("Negative trap damage"); 
    }
}

void Trap::trigger(Hero& h) {
    if (!revealed) { 
        h.takeDamage(damage); revealed = true; 
    }
}

bool Trap::isRevealed() const{
    return revealed;
}