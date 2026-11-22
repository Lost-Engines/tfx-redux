#pragma once

#include "flight/arcade.h"
#include "flight/landing_gear.h"
#include "flight/simple_controls.h"
#include "flight/simple_ground.h"
#include "flight/simple_turn.h"

namespace tfx::flight {

struct simple_state {
  arcade_state flight{};
  simple_control_state controls{};
  simple_turn_state turn{};
  simple_ground_state ground{};
  landing_gear_state gear{};
  float geometric_altitude_km{0};
  float displayed_g_load{0};
  float reported_speed{0.5f};
  bool first_update{true};
};
struct simple_inputs {
  flight_controls controls{};
  control_source source{control_source::keyboard};
  direction_keys keys{};
  vec2<int16_t> joystick{0, 0};
  bool alt{false};
  int16_t surface_type{0};
};
struct simple_update {
  int16_t horizon_motion{0};
  std::vector<unsigned int> gear_sounds{};
};

void initialise_simple(simple_state &state, vec3<int32_t> const &position, vec3<math::angle> const &angles, float speed, bool landed, ground_contact ground = {});
void advance_simple_disabled(simple_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, ground_contact ground = {});
simple_update advance_simple(simple_state &state, simple_inputs const &inputs, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, ground_contact ground = {});

} // namespace tfx::flight
