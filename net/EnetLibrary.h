#ifndef DUNGEON_PARTY_NET_ENET_LIBRARY_H
#define DUNGEON_PARTY_NET_ENET_LIBRARY_H

#include <enet/enet.h>
#include <stdexcept>

namespace net {

// enet_initialize()/enet_deinitialize() must wrap all ENet use (on Windows they start/stop Winsock).
// Every NetClient/RoomServer holds one of these; the library stays up while at least one exists.
class EnetLibrary {
public:
    EnetLibrary() {
        if (refs()++ == 0 && enet_initialize() != 0) {
            --refs();
            throw std::runtime_error("Could not initialize ENet");
        }
    }
    ~EnetLibrary() {
        if (--refs() == 0) enet_deinitialize();
    }
    EnetLibrary(const EnetLibrary&) = delete;
    EnetLibrary& operator=(const EnetLibrary&) = delete;

private:
    static int& refs() { static int count = 0; return count; }
};

} // namespace net

#endif
