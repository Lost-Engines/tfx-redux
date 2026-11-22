#include "model_metrics.h"
#include <utility>
#include "colour_remap.h"

namespace tfx::render {
namespace {
int32_t wrap(uint32_t const value) {
  return std::bit_cast<int32_t>(value);
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? wrap(0u - static_cast<uint32_t>(value)) : value;
}
}

model_metrics measure_model(vec3<int32_t> const &camera_origin, int16_t const sort_bias) {
  int32_t largest{magnitude(camera_origin.x)};
  int32_t other{magnitude(camera_origin.y)};
  int32_t third{magnitude(camera_origin.z)};
  if(third >= largest) std::swap(third, largest);
  if(other >= largest) std::swap(other, largest);
  // The original estimates max(|axis|) + (the other two axes)/4 before
  // taking its signed high word. Dword ADD and NEG deliberately wrap.
  int32_t const remainder{wrap(static_cast<uint32_t>(other) + static_cast<uint32_t>(third)) >> 2};
  int32_t const distance{wrap(static_cast<uint32_t>(largest) + static_cast<uint32_t>(remainder)) >> 16};
  return {
    .distance{math::signed_word(static_cast<uint16_t>(distance))},
    .order{math::signed_word(static_cast<uint16_t>((camera_origin.z >> 16) + sort_bias))},
    .colour_row{distance_colour_row(camera_origin.z)},
  };
}

} // namespace tfx::render
