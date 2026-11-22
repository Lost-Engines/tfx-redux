#pragma once

#include "vectorstorm/vector/vector2.h"
#include "flight/aerodynamic_pose.h"

namespace tfx::flight {

struct aerodynamic_motion {
  aerodynamic_pose pose{};
  vec3d velocity_ned{0,0,0};
  vec3d acceleration_ned{0,0,0};
  vec3d ground_velocity_body{0,0,0};
  vec3d air_velocity_body{0,0,0};
  vec3d angular_rate{0,0,0};                                                   // Local roll, pitch, yaw, in radians per second.
  vec2d angular_acceleration{0,0};
  vec2d rotation_remainder{0,0};                                               // Roll/pitch in original 65,536-unit angles.
  double yaw_step_radians{0};
  int16_t collision{0};
  int32_t integer_world_y{0};
};
struct aerodynamic_motion_context {
  double seconds{.02};
  bool time_warp{false};
  bool landed{false};
  bool crashing{true};
  bool simplified_rotation{false};
  bool roll_rate_inhibited{false};                                             // Native refuelling-connection phase.
  double commanded_roll_rate{0};
  double commanded_angle_of_attack{0};
  double angle_of_attack{0};
};

// Native 0x9f9d4 / 0x9fbec: position, collision fallback and body rotation.
// Velocity has already advanced; the half-acceleration correction avoids
// counting the current acceleration twice in the position/orientation step.
void advance_aerodynamic_motion(aerodynamic_motion &state, aerodynamic_motion_context const &context);

} // namespace tfx::flight
