#pragma once

#include "cockpit/hud.h"
#include "flight/fuel_gauge.h"

namespace tfx::cockpit {

struct instrument_setup {
  hud_selection displays;
  vec2<int16_t> offset;
  std::array<flight::system_condition, flight::aircraft_system_count> systems;
  flight::fuel_gauge fuel;
  uint8_t chaff;
  uint8_t flares;
  bool stealth;
};
instrument_setup initialise_instruments(flight::player_aircraft aircraft, bool landed);

} // namespace tfx::cockpit
