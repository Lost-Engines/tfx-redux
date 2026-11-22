#pragma once

#include "flight/aerodynamic_motion.h"

namespace tfx::flight {

struct aerodynamic_basis {
  vec3d forward{1, 0, 0};
  vec3d right{0, 1, 0};
  vec3d down{0, 0, 1};
};
struct aerodynamic_loads {
  vec3d aerodynamic_force{0, 0, 0};
  vec3d engine_force{0, 0, 0};
  vec3d ground_force{0, 0, 0};
  vec3d aerodynamic_moment{0, 0, 0};
  vec3d engine_moment{0, 0, 0};
  vec3d ground_moment{0, 0, 0};
};
struct aerodynamic_inertia {
  double mass_kg{0};
  double roll_moment{0};
  double pitch_moment{0};
  double fuel_kg{0};
};
struct aerodynamic_force_context {
  aerodynamic_basis basis{};
  aerodynamic_loads loads{};
  aerodynamic_inertia inertia{};
  double cockpit_upward_metres{-.85344};
  bool refuelling_connected{false};
  bool fully_arrested{false};
};
struct aerodynamic_force_result {
  vec3d body_force{0, 0, 0};
  vec3d ned_force{0, 0, 0};
  vec3d body_moment{0, 0, 0};
  float g_load{0};
};

// Native 0x9f128 and 0x9f3b0 sum local loads and gravity, transform to NED,
// and update linear/roll/pitch accelerations using the original inertia terms.
aerodynamic_force_result update_aerodynamic_forces(aerodynamic_motion &state, aerodynamic_force_context const &context);

} // namespace tfx::flight
