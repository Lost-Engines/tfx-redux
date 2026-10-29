#pragma once
#include "render/continuous_pose.h"
#include <algorithm>

namespace tfx::cockpit {

// Pitch ladder geometry before rasterisation. Keep the authored 1820-unit
// ten-degree spacing, while removing Q15 sine, depth and screen rounding.
inline vec2d pitch_mark_position(unsigned const index,vec2d const &projection_angles,vec2d const &origin,double const focal_distance) {
  unsigned const ordinal{(index+33)%36}; // Three wraparound marks on each side.
  double const mark{ordinal*1820.0*(2*std::numbers::pi/65536)};
  auto const [sm,cm]=render::sin_cos(mark);
  auto const [sp,cp]=render::sin_cos(projection_angles.x);
  auto const [sr,cr]=render::sin_cos(projection_angles.y);
  double const depth{std::max(1.0/128,cm*cp-sm*sp)};
  double const height{(sm*cp+cm*sp)*focal_distance/depth};
  return origin+vec2d{std::clamp(height*sr,-32000.0,32000.0),std::clamp(height*cr,-32000.0,32000.0)};
}
inline unsigned pitch_mark_first(double pitch) {
  pitch=std::fmod(pitch,2*std::numbers::pi);if(pitch<0)pitch+=2*std::numbers::pi;
  return std::min(37u,static_cast<unsigned>(pitch*(65536/(2*std::numbers::pi))/1820)+1);
}

} // namespace tfx::cockpit
