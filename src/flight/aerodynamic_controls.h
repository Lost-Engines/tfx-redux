#pragma once

#include "flight/aerodynamic_powerplant.h"
#include "flight/control_input.h"

namespace tfx::flight {

struct aerodynamic_autopilot_targets {
  bool enabled{false};
  uint16_t mode{0};                                                            // Original mode codes; 0/1 allow target-altitude input.
  double down_metres{0};
  math::angle heading{0};
  double metres_per_second{0};
};
struct aerodynamic_control_state {
  double elevator{0};
  double aileron{0};
  double rudder{0};
  double keyboard_elevator{0};
  double keyboard_aileron{0};
  double keyboard_rudder{0};
  double minimum_elevator{0};
  double maximum_elevator{0};
  double aileron_limit{0};
  aerodynamic_autopilot_targets autopilot{};
};
struct aerodynamic_control_context {
  control_source source{control_source::keyboard};
  direction_keys keys{};
  vec2<int16_t> joystick{0, 0};
  vec2<int16_t> mouse{0, 0};                                                   // Original centered absolute samples, not deltas.
  double seconds{.02};
  double metres_per_second{0};
  double pitch_gain{0};
  double roll_gain{0};
  double rudder_increment{0};
  double joystick_gain{1};
  double elevator_trim{0};
  double maximum_autopilot_speed{0};
  bool alt{false};
  bool low_speed_ground_contact{false};
  bool accelerate{false};
  bool decelerate{false};
};

// Native 0x9a104. Returns the ordered afterburner-stop notification trigger;
// device acquisition supplies samples separately from these numerical rules.
bool update_aerodynamic_controls(aerodynamic_control_state &state, aerodynamic_powerplant &powerplant, aerodynamic_control_context const &context);

} // namespace tfx::flight
