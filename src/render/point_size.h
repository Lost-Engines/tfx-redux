#pragma once

#include <algorithm>
#include <cmath>
#include "models/shape.h"

namespace tfx::render {

// One pixel at small resolutions, two across at 1080p, three at 2160p.
// Use the shorter framebuffer side so a very wide window does not inflate dots.
inline double object_point_diameter(vec2<int> const &size) {
  return std::max(1.0,std::min(size.x,size.y)/720.0);
}
inline double point_diameter(models::primitive const &point, vec2<int> const &size,
                             double focal_distance, double distance) {
  if(point.point_role==models::point_role::object) return object_point_diameter(size);
  double const lens_scale{std::max(1.0,focal_distance/512.0)};
  if(point.point_role==models::point_role::particle) return point.pixel_size*lens_scale;
  // Lights have a bounded near-field glow, then tend towards inverse-distance
  // size. The small resolution-based floor keeps remote runway/city lights visible.
  double const floor{std::max(1.0,std::min(size.x,size.y)/1440.0)};
  double constexpr glow_distance{800}; // World units, approximately two kilometres.
  return std::max(floor,point.pixel_size*lens_scale*glow_distance/std::hypot(glow_distance,distance));
}

} // namespace tfx::render
