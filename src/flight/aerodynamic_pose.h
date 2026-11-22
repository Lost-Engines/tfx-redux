#pragma once

#include "math/attitude.h"
#include "flight/aircraft.h"

namespace tfx::flight {

// The full aerodynamic model uses North/East/Down coordinates. Its local
// columns are forward/right/down; the renderer instead stores right/down/forward.
struct aerodynamic_frame {
  vec3<int32_t> forward{32767, 0, 0};
  vec3<int32_t> right{0, 32767, 0};
  vec3<int32_t> down{0, 0, 32767};
  void normalise();
  math::attitude world_basis() const;
};
struct aerodynamic_pose {
  aerodynamic_frame frame{};
  vec3d metres_ned{0, 0, 0};
};
struct aerodynamic_viewpoint {
  double forward{0};
  double upward{0};
};
struct aerodynamic_view {
  math::attitude orientation{};
  vec3f position{0, 0, 0};
  vec3<int32_t> integer_position{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
};

// Native 0x95aa0 normalises the physical frame and exports its world/view pose.
// Viewpoint offsets are already in original world units, separately from metres.
aerodynamic_view update_aerodynamic_view(aerodynamic_pose &state, player_aircraft aircraft, aerodynamic_viewpoint const &viewpoint);

} // namespace tfx::flight
