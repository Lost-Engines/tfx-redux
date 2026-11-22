#pragma once

#include "flight/aerodynamic_coefficients.h"
#include "flight/damage.h"

namespace tfx::flight {

struct aerodynamic_surface_parameters {
  double wing_area{0};
  double wing_span{0};
  double mean_chord{0};
  double flap_lift_gradient{0};
  double maximum_flap_angle{0};
  double flap_limit_gradient{0};
  double maximum_lift{0};
  double automatic_flap_gradient{0};
  double maximum_automatic_flap_angle{0};
  double zero_lift{0};
  double critical_angle{0};
  double induced_drag{0};
  double air_brake_drag{0};
  double gear_drag{0};
  double elevator_drag{0};
  double chute_area{0};
  double zero_pitch_moment{0};
  double chute_pitch_arm{0};
  vec2d air_brake_moment_arm{0, 0};                                            // Original sin(alpha), cos(alpha) terms.
  double maximum_aileron_angle{0};
  double maximum_elevator_angle{0};
};
struct aerodynamic_surface_state {
  double automatic_flap_fraction{0};
  double flap_lift_offset{0};
  double positive_lift_limit{0};
  double negative_lift_limit{0};
  double wing_lift_coefficient{0};
  double effective_angle{0};
  double lift_coefficient{0};
  double lift{0};
  double drag_coefficient{0};
  double drag{0};
  double pitch_rate_moment_term{0};
  double pitch_moment{0};
  double roll_rate_moment_term{0};
  double roll_moment{0};
  vec3d force{0, 0, 0};
  vec3d moment{0, 0, 0};
  bool stalled{false};
};
struct aerodynamic_surface_context {
  player_aircraft aircraft{player_aircraft::eurofighter};
  aerodynamic_coefficients coefficients{};
  double angle_of_attack{0};
  double metres_per_second{0};
  double density{0};
  double dynamic_pressure{0};
  double flap_fraction{0};
  double gear_fraction{0};
  double air_brake_fraction{0};
  double elevator{0};
  double aileron{0};
  double sideslip{0};
  double pitch_rate{0};
  double roll_rate{0};
  bool chute_deployed{false};
  bool calculate_moments{true};
  system_condition flaps_condition{system_condition::normal};
};

// Native 0x9c7f0 and the corresponding simplified branch: lift/stall/flaps,
// drag, pitch/roll moments and their projection into body force/moment axes.
void update_aerodynamic_surfaces(aerodynamic_surface_state &state, aerodynamic_surface_parameters const &parameters, aerodynamic_surface_context const &context);

} // namespace tfx::flight
