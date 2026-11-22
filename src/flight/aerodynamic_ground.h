#pragma once

#include "flight/aerodynamic_contact.h"

namespace tfx::flight {

enum class aerodynamic_arrestor_phase : uint16_t {
  free,
  engaged,
  stopped
};

struct aerodynamic_ground_parameters {
  aerodynamic_contact_parameters contact{};
  std::array<vec3d, 6> offsets{};                                              // Nose, left/right main gear, then three body points.
  vec3d hook_offset{0, 0, 0};
  double maximum_gear_speed{0};
  double arrestor_stiffness{0};
  double arrestor_damping{0};
};
struct aerodynamic_ground_state {
  aerodynamic_contact_state contact{};
  vec3d hook_offset_ned{0, 0, 0};
  vec3d hook_anchor_ned{0, 0, 0};
  vec3d hook_displacement{0, 0, 0};
  vec3d hook_force_ned{0, 0, 0};
  vec3d hook_force_body{0, 0, 0};
  vec3d hook_moment{0, 0, 0};
  double cable_length{0};
  double cable_speed{0};
  double rollout_distance{0};
  aerodynamic_arrestor_phase arrestor{aerodynamic_arrestor_phase::free};
  aerodynamic_contact_kind last_contact_kind{aerodynamic_contact_kind::body};
  bool ground_steering{false};
  bool landed{false};
};
struct aerodynamic_ground_context {
  aerodynamic_contact_context contact{};
  vec3d previous_ned_force{0, 0, 0};
  vec2d engine_factor{1, 1};
  double gear_fraction{0};
  double metres_per_second{0};
  double seconds{.02};
  float throttle{0};
  bool non_arcade_model{true};
  bool carrier_surface{false};
  bool arrestor_hook_down{false};
};
struct aerodynamic_ground_events {
  unsigned int gear_impacts{0};
  bool cockpit_bump{false};
};

// Native military-spec 0x9ce5c, including all six 0x9d734 evaluations, shared
// random/damage state, ground start assistance and carrier arresting forces.
aerodynamic_ground_events update_aerodynamic_ground(aerodynamic_ground_state &state, aerodynamic_motion &motion, damage_state &damage, math::random_state &random, aerodynamic_ground_parameters const &parameters, aerodynamic_ground_context const &context);

} // namespace tfx::flight
