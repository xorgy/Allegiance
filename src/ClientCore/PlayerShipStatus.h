#pragma once
#include <cstdint>

namespace Allegiance {
// Value counterpart of IGC ShipStatus. Explicit bytes avoid its compiler-
// dependent bitfields, padding and uninitialized parent ID on the wire.
struct PlayerShipStatus {
    enum State : uint8_t {Dead=1,Docked,Flying,Observer,Turret};
    enum Flag : uint8_t {Unknown=1,Detected=2};
    uint32_t stateTime{};
    int16_t hull{-1},sector{-1},station{-1},parent{-1};
    uint8_t state{Dead},flags{Unknown};uint16_t reserved{};
    bool current()const{return !(flags&Unknown);}
    bool operator==(const PlayerShipStatus&)const=default;
};
static_assert(sizeof(PlayerShipStatus)==16);

inline void ObservePlayerShip(PlayerShipStatus& status,PlayerShipStatus::State state,
                              int hull,int sector,int station,int parent,uint32_t now) {
    // Warping and changing hull do not restart a state's clock. A new sighting,
    // docking or change between pilot/observer/turret does.
    if(!status.current() || status.state!=state || (state==PlayerShipStatus::Docked && status.station!=station))
        status.stateTime=now;
    status.hull=int16_t(hull);status.sector=int16_t(sector);
    status.station=int16_t(station);status.parent=int16_t(parent);status.state=state;
    status.flags&=~PlayerShipStatus::Unknown;
}
inline void HidePlayerShip(PlayerShipStatus& status){status.flags|=PlayerShipStatus::Unknown;}
}
