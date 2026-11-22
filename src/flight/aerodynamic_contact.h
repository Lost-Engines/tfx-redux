#pragma once

#include "flight/aerodynamic_forces.h"
#include "flight/damage.h"

namespace tfx::flight {

enum class aerodynamic_contact_kind { body, wheel, retracted_wheel };

struct aerodynamic_contact_parameters {
  double spring_stiffness{0};
  double damping{0};
  double maximum_sink_speed{0};
  double rolling_warning_speed{0};
  double rolling_failure_speed{0};
  double brake_warning_speed{0};
  double brake_failure_speed{0};
  double braking_coefficient{0};
  double wet_rolling_coefficient{0};
  double wet_braking_factor{0};
  double empty_mass_kg{0};
  double left_main_wheel_right{0};
  double unloaded_clearance{0};
};
struct aerodynamic_contact_state {
  vec3d offset_body{0,0,0};
  vec3d point_ned{0,0,0};
  vec3d velocity_body{0,0,0};
  vec3d velocity_ned{0,0,0};
  vec3d working_force{0,0,0};
  vec3d force{0,0,0};
  vec3d moment{0,0,0};
  double stiffness{0};
  double damping{0};
  double penetration{0};
  double down_velocity{0};
  double rolling_coefficient{0};
  double braking_coefficient{0};
  double resistance_force{0};
  double unloaded_clearance{0};
  double ground_up_metres{0};
};
struct aerodynamic_contact_context {
  aerodynamic_basis basis{};
  aerodynamic_contact_kind kind{aerodynamic_contact_kind::body};
  int16_t ground_elevation_steps{0};                                           // Original signed terrain height, in eight-foot steps.
  int16_t body_downward_q15{0};
  double total_mass_kg{0};
  bool terrain_present{false};
  bool gear_damage_enabled{true};
  bool low_speed_ground_contact{false};
  bool precipitation{false};
  bool rough_surface{false};                                                   // Original ground-zone classification 2.
  bool wheel_brakes{false};
};

// Native 0x9d734: one gear/body point, accumulating suspension, friction and
// braking loads. Retained caches, exact damage timing and random bump order
// matter when the caller evaluates the six contact points in sequence.
// Returns the original failed-gear impact notification request.
bool update_aerodynamic_contact(aerodynamic_contact_state &state, aerodynamic_motion &motion, damage_state &damage, math::random_state &random, aerodynamic_contact_parameters const &parameters, aerodynamic_contact_context const &context);

} // namespace tfx::flight
