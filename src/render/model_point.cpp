#include "model_point.h"
#include <limits>

namespace tfx::render {

bool model_point(framebuffer &output, vec3<int32_t> const &camera_origin, camera const &view, unsigned int const pixel_size, uint8_t const colour) {
  int32_t const depth{camera_origin.z >> 16};
  int16_t constexpr near_depth{16};
  if(depth <= near_depth) return true;
  // Redux uses the same aperture for distant points and polygon models.
  unsigned int const shift{projection_shift(view.opening)};
  vec2<int32_t> const quotient{(camera_origin.x >> shift) / depth,(camera_origin.y >> shift) / depth};
  if(quotient.x < std::numeric_limits<int16_t>::min() || quotient.x > std::numeric_limits<int16_t>::max() || quotient.y < std::numeric_limits<int16_t>::min() || quotient.y > std::numeric_limits<int16_t>::max()) return false;
  vec2<int16_t> const point{
    math::signed_word(static_cast<uint16_t>(quotient.x + static_cast<int32_t>(view.projection_origin.x))),
    math::signed_word(static_cast<uint16_t>(quotient.y + static_cast<int32_t>(view.projection_origin.y))),
  };
  if(point.x < view.viewport_minimum.x || point.x > view.viewport_maximum.x || point.y < view.viewport_minimum.y || point.y > view.viewport_maximum.y) return true;
  for(unsigned int y{0}; y != pixel_size; ++y) for(unsigned int x{0}; x != pixel_size; ++x) {
    // Clip the complete block: the original clips only its anchor, allowing
    // a right-edge second pixel to wrap into the following VGA row.
    int const offset{(point.y + static_cast<int>(y)) * output.size.x + point.x + static_cast<int>(x)};
    if(point.x + static_cast<int>(x) < output.size.x && point.y + static_cast<int>(y) < output.size.y && offset >= 0 && offset < static_cast<int>(output.pixels.size())) output.write(static_cast<unsigned int>(offset),colour);
  }
  return true;
}

} // namespace tfx::render
