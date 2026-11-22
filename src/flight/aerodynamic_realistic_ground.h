#pragma once

#include "flight/aerodynamic_controls.h"
#include "flight/aerodynamic_ground.h"

namespace tfx::flight {

struct aerodynamic_realistic_ground_state {
  aerodynamic_ground_state ground{};
  vec3d wheel_penetration{0,0,0};                                              // Nose, left main, right main.
  double roll_angle{0};
  double pitch_angle{0};
  double commanded_roll_rate{0};
  double commanded_angle_of_attack{0};
};
struct aerodynamic_realistic_ground_context {
  aerodynamic_ground_context ground{};
  double stall_metres_per_second{0};
  double angle_of_attack{0};
  double lift{0};
};

// Complete native 0x9e0f4, including its three projection-only 0x9f014 calls,
// kinematic touchdown/settling corrections, braking and shared carrier cable.
aerodynamic_ground_events update_aerodynamic_realistic_ground(aerodynamic_realistic_ground_state &state, aerodynamic_motion &motion, aerodynamic_control_state &controls, damage_state &damage, math::random_state &random, aerodynamic_ground_parameters const &parameters, aerodynamic_realistic_ground_context const &context);

} // namespace tfx::flight
