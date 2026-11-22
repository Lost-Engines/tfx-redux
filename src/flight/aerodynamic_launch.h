#pragma once

#include "flight/aerodynamic.h"

namespace tfx::flight {

struct aerodynamic_launch_context {
  vec3<int32_t> position{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
  aerodynamic_model model{aerodynamic_model::military_spec};
  unsigned int player_route_points{0};
  int16_t ground_elevation_steps{0};
  bool landed{false};
  aerodynamic_wind_context wind{};
  std::optional<int16_t> surface_celsius{};
};

// Native player setup 0x8c184, engine priming 0x8c318 and the first-placement
// prefix of 0x8f56c is applied separately. Caller supplies the separately verified instrument/system
// startup conditions. The first ordinary callback still sees the prior-model
// marker and performs its entry-time physical reset.
aerodynamic_flight launch_aerodynamic_flight(aerodynamic_airframe const &aircraft, loadout const &stores, aerodynamic_launch_context const &context, aerodynamic_wind_parameters const &wind_parameters, damage_state &damage, math::random_state &random);

// Native 0x8f56c..0x8f686: once, at the beginning of the first player callback.
void place_aerodynamic_start(aerodynamic_flight &state, int16_t ground_elevation_steps);

} // namespace tfx::flight
