#pragma once

#include "math/attitude.h"
#include "flight/control_input.h"
#include "simulation/timing.h"

namespace tfx::flight {

struct simple_control_state {
  vec3f angular_rate{0, 0, 0};                                                 // Pitch, heading and roll in the native angle units.
  float negative_pitch_limit{0};
  float positive_pitch_limit{0};
  float roll_limit{0};
};
struct simple_control_context {
  int32_t fixed_speed{0};
  float metres_per_second{0};
  double maximum_g{9};
  double minimum_g{-3};
  double stall_metres_per_second{64.375};
  float coordinated_turn_rate{0};
  math::attitude const &orientation;
  simulation::timer_ticks elapsed{0};
  control_source source{control_source::keyboard};
  vec2<int16_t> joystick{0, 0};
  direction_keys keys{};
  bool landed{false};
  bool low_speed_ground_contact{false};
  bool pitch_inhibited{false};
  bool alt{false};                                                             // Native raw-key table scan code 0x38 enables rudder steering.
};

// Native 0x8cfa0; returns the effective direction keys set by joystick thresholding.
// This is the Simple model's response stage, before orientation and position updates.
direction_keys update_simple_controls(simple_control_state &state, simple_control_context const &context);

} // namespace tfx::flight
