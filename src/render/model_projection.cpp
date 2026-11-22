#include "model_projection.h"
#include <cmath>
#include <limits>
#include "near_clip.h"

namespace tfx::render {

std::optional<vec3<int32_t>> model_camera_point(models::model_vertex const &vertex) {
  if(vertex.projection_scale <= 0) return std::nullopt;
  vec3<int32_t> point;
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const fixed{std::nearbyint(vertex.position[axis] / vertex.projection_scale * 65536)};
    if(!std::isfinite(fixed) || fixed < std::numeric_limits<int32_t>::min() || fixed > std::numeric_limits<int32_t>::max()) return std::nullopt;
    point[axis] = static_cast<int32_t>(fixed);
  }
  return point;
}

std::optional<vec2<int16_t>> project_model_point(vec3<int32_t> const &point, camera const &view) {
  int32_t const depth{point.z >> 16};
  if(depth <= model_near_depth) return std::nullopt;
  unsigned int const shift{projection_shift(view.opening)};
  vec2<int32_t> const quotient{(point.x >> shift) / depth, (point.y >> shift) / depth};
  if(quotient.x < std::numeric_limits<int16_t>::min() || quotient.x > std::numeric_limits<int16_t>::max() || quotient.y < std::numeric_limits<int16_t>::min() || quotient.y > std::numeric_limits<int16_t>::max()) return std::nullopt;
  return vec2<int16_t>{math::signed_word(static_cast<uint16_t>(quotient.x + static_cast<int32_t>(view.projection_origin.x))), math::signed_word(static_cast<uint16_t>(quotient.y + static_cast<int32_t>(view.projection_origin.y)))};
}

} // namespace tfx::render
