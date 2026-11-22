#pragma once

#include "flight/arcade.h"

namespace tfx::flight {

struct simple_ground_state {
  math::attitude body{};
  math::attitude visible{};
  vec3f rotation_remainder{0, 0, 0};
  float position_y{0};
  int16_t stall_pitch{0};
  vec2<int16_t> landing_angles{0, 0};
  bool landed{false};
  bool previous_landed{false};
  bool low_speed_ground_contact{false};
  int16_t collision{0};
  bool arrested{false};
  double indicated_speed{0};
  double low_speed_reference{0};
};
struct simple_ground_context {
  simulation::timer_ticks elapsed{0};
  ground_contact ground{};
  float gear_height{0};
  float metres_per_second{0};
  float turn_metres_per_second{0};
  float reported_speed{0};
  double stall_metres_per_second{0};
  double low_speed_reference{0};
  double rollout_speed_threshold{30};
  int32_t fixed_speed{0};
  bool gear_down{false};
  bool pitch_inhibited{false};
  bool pitch_down{false};
  int16_t surface_type{0};
  bool arrestor_hook{false};
};

// Native 0x8d6e8 only changes the visible basis and retained stall pitch.
void update_simple_stall_attitude(simple_ground_state &state, simple_ground_context const &context);
// Native 0x8d970 can rotate the body basis during touchdown.
void update_simple_ground(simple_ground_state &state, simple_ground_context const &context);

} // namespace tfx::flight
