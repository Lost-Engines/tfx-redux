#pragma once

#include "simulation/timing.h"

namespace tfx::flight {

enum class player_callback {
  none,
  arcade,
  simple,
  military_spec,
  realistic,
  automatic_landing,
  disabled_aircraft,
  pilot
};
struct player_dispatch_context {
  // Native 0x86ce disables the aircraft; nonzero pilot phase 0xe784 selects
  // the separately reconstructed 0x8f198 ejected-pilot controller.
  simulation::flight_model model{simulation::flight_model::arcade};
  simulation::timer_ticks elapsed{0};
  float autopilot_military_tick_limit{4};
  bool autopilot{false};
  bool automatic_landing{false};
  bool aircraft_disabled{false};
  bool pilot_active{false};
};
struct player_dispatch {
  player_callback callback{player_callback::none};
  bool clear_autopilot{false};
  bool clear_automatic_landing{false};
};

// Native 0x8f8e2..0x8f98c: configured mode and active callback differ when
// Arcade/Simple autopilot selects an aerodynamic path by elapsed-frame length.
player_dispatch select_player_callback(player_dispatch_context const &context);

} // namespace tfx::flight
