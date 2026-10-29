#pragma once

#include "math/attitude.h"
#include <cmath>
#include <numbers>

namespace tfx::render {

// Presentation-only transforms. Simulation and native reference paths retain
// their original 1,024-phase tables and fixed-point arithmetic.
inline double angle_radians(math::angle const angle) {
  return math::signed_word(angle) * (2 * std::numbers::pi / 65536);
}
inline vec3d angle_radians(vec3<math::angle> const &angles) {
  return {angle_radians(angles.x),angle_radians(angles.y),angle_radians(angles.z)};
}
inline mat3d continuous_rotation(vec3d const &radians) {
  auto const sp{std::sin(radians.x)}, cp{std::cos(radians.x)};
  auto const sh{std::sin(radians.y)}, ch{std::cos(radians.y)};
  auto const sr{std::sin(radians.z)}, cr{std::cos(radians.z)};
  mat3d result;
  result.data = {
    ch*cr+sh*sp*sr, -cp*sr, -sh*cr+ch*sp*sr,
    ch*sr-sh*sp*cr, cp*cr, -sh*sr-ch*sp*cr,
    sh*cp, sp, ch*cp,
  };
  return result;
}
inline mat3d continuous_rotation(vec3<math::angle> const &angles) {
  return continuous_rotation(angle_radians(angles));
}
inline mat3d continuous_basis(math::attitude const &basis) {
  // Remove Q15 length drift without reducing the axes to native Euler phases.
  auto const forward{vec3d{basis.forward}.normalise_copy()};
  auto const right{vec3d{basis.down}.cross(forward).normalise_copy()};
  auto const down{forward.cross(right)};
  mat3d result;
  result.data={right.x,right.y,right.z,down.x,down.y,down.z,forward.x,forward.y,forward.z};
  return result;
}
inline vec3d continuous_angles(mat3d const &basis) {
  auto const right{basis*vec3d{1,0,0}}, forward{basis*vec3d{0,0,1}};
  if(std::hypot(forward.x,forward.z)<1e-12)
    return {std::copysign(std::numbers::pi/2,forward.y),std::atan2(-right.z,right.x),0};
  return {
    std::atan2(forward.y,std::hypot(forward.x,forward.z)),
    std::atan2(forward.x,forward.z),
    std::atan2(-right.y,right.x*forward.z-right.z*forward.x),
  };
}
inline double wrapped_radians(double const value) {
  double const result{std::remainder(value,2*std::numbers::pi)};
  // Native signed-word differences choose the negative arc at exactly 180°.
  return result>=std::numbers::pi ? result-2*std::numbers::pi : result;
}
inline vec3d follow_angles(vec3d const &previous, vec3d const &target, double const fraction, bool const tracking_roll) {
  auto result{previous};
  for(unsigned axis{0};axis!=3;++axis)
    result[axis]+=wrapped_radians(target[axis]-previous[axis])*fraction;
  if(tracking_roll) result.z=target.z;
  // Keep long-running orbits bounded, including across the 0/360 seam.
  for(unsigned axis{0};axis!=3;++axis) result[axis]=wrapped_radians(result[axis]);
  return result;
}

} // namespace tfx::render
