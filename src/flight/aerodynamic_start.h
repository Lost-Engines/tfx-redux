#pragma once

#include "flight/aerodynamic_airframes.h"

namespace tfx::flight {

struct aerodynamic_start_state {
  aerodynamic_motion motion{};
  store_effects stores{};
  vec2d engine_factor{0, 0};
  double fuel_kg{0};
  double mass_kg{0};
  double seconds{0};
  float g_load{0};
  int32_t track_heading{0};
  bool ground_steering{false};
};
struct aerodynamic_start_context {
  math::attitude orientation{};
  vec3f position{0, 0, 0};
  int32_t fixed_speed{0};
  math::angle heading{0};
  int16_t ground_elevation_steps{0};
  bool landed{false};
  bool cold_start{false};
  bool terrain_present{false};
};

// Native 0x954fc: rebuild the physical frame/pose and starting velocities when
// entering an advanced model. The existing store state precedes recalculation,
// matching the native mass/store ordering even when the loadout changed.
void initialise_aerodynamic_motion(aerodynamic_start_state &state, aerodynamic_airframe &airframe, loadout const &stores, aerodynamic_start_context const &context);

} // namespace tfx::flight
