#pragma once

#include "weapons/projectile.h"

namespace tfx::weapons {

// Native 0xa2878 samples a temporary cannon trajectory for the HUD. It never
// inserts the predicted shot into the live projectile pool or consumes ammo.
std::array<vec3<int32_t>, 5> predict_cannon(definition const &cannon, vec3<int32_t> const &anchor_q8,
  vec3<math::angle> const &angles, int32_t speed, vec2<int32_t> const &height_bounds);
std::array<vec3<int32_t>, 5> predict_cannon(definition const &cannon, projectile shot, vec2<int32_t> const &height_bounds);

} // namespace tfx::weapons
