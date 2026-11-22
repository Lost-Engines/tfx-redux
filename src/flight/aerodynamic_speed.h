#pragma once

#include "flight/aerodynamic_powerplant.h"

namespace tfx::flight {

enum class aerodynamic_speed_strategy {
  maintain,
  recover
};
struct aerodynamic_speed_state {
  double target_metres_per_second{0};
  double air_brake_fraction{0};
  system_condition air_brake_condition{system_condition::normal};
};
struct aerodynamic_speed_context {
  double metres_per_second{0};
  double mass_kg{0};
  double forward_force{0};
  double air_brake_drag{0};
  unsigned int afterburner_stages{0};
  aerodynamic_speed_strategy strategy{aerodynamic_speed_strategy::maintain};
};

// Native 0x9aa4c / 0x9ab78: autopilot/overspeed and recovery thrust control.
// Returned stage-change semantics drive separate sound/model/device consumers.
afterburner_change update_aerodynamic_speed(aerodynamic_speed_state &state, aerodynamic_powerplant &powerplant, aerodynamic_speed_context const &context);

} // namespace tfx::flight
