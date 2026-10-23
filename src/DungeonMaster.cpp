#include "DungeonMaster.h"
#include "DMCard.h"
#include <stdexcept>
bool DungeonMaster::acceptsCard(const Card& c) const { 
    return dynamic_cast<const DMCard*>(&c) != nullptr; 
}
void DungeonMaster::setTraps(const Card&) { 
    throw std::logic_error("Trap placement/trigger rules pending"); 
}
