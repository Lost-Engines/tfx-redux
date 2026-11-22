#include "near_clip.h"

namespace tfx::render {
namespace {
int16_t word(int32_t const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
int32_t midpoint(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)) >> 1;
}

template<unsigned int Count> struct near_vertex {
  vec2<int32_t> position;
  std::array<uint16_t,Count> attributes;
};
template<unsigned int Count>
near_vertex<Count> intersect(vec3<int32_t> const &outside, vec3<int32_t> const &inside, std::array<uint16_t,Count> const &outside_attributes, std::array<uint16_t,Count> const &inside_attributes) {
  int16_t lower{word(outside.z >> 16)}, upper{word(inside.z >> 16)};
  if(lower == model_near_depth) return {
    .position{outside.x,outside.y},
    .attributes{outside_attributes},
  };
  uint16_t path{0};
  // Original unrolled search bisects signed WORD depths, including overflow
  // before SAR. It records branch decisions low bit first. Equality adds a
  // sentinel next bit; the sixteenth comparison has no room for that sentinel.
  unsigned int constexpr depth_bits{16};
  for(unsigned int step{0}; step != depth_bits; ++step) {
    int16_t const centre{static_cast<int16_t>(word(lower + upper) >> 1)};
    if(centre == model_near_depth) {
      if(step != depth_bits - 1) path = static_cast<uint16_t>(path | (3u << step));
      break;
    }
    if(centre > model_near_depth) { upper = centre;path = static_cast<uint16_t>(path | (1u << step)); } else lower = centre;
  }
  vec2<int32_t> a{outside.x,outside.y}, b{inside.x,inside.y};
  auto attributes_a{outside_attributes},attributes_b{inside_attributes};
  bool upper_half{(path & 1) != 0};path >>= 1;
  for(;;) {
    auto &endpoint{upper_half ? b : a};
    endpoint = {midpoint(a.x,b.x),midpoint(a.y,b.y)};
    auto &attributes{upper_half ? attributes_b : attributes_a};
    for(unsigned int i{0}; i != Count; ++i) attributes[i] = static_cast<uint16_t>((static_cast<uint32_t>(attributes_a[i]) + attributes_b[i]) >> 1);
    upper_half = (path & 1) != 0;path >>= 1;
    // The last set bit terminates replay; it is not another midpoint step.
    // The native loop still performs one midpoint when its path word is zero.
    if(!path) return {
      .position{vec2<int32_t>{endpoint}},
      .attributes{attributes},
    };
  }
}

} // anonymous namespace

shaded_near_vertex intersect_shaded_near_plane(vec3<int32_t> const &outside, vec3<int32_t> const &inside, uint16_t const outside_shade, uint16_t const inside_shade) {
  auto const result{intersect<1>(outside,inside,{outside_shade},{inside_shade})};
  return {
    .position{vec2<int32_t>{result.position}},
    .shade_q8{result.attributes[0]},
  };
}
textured_near_vertex intersect_textured_near_plane(vec3<int32_t> const &outside, vec3<int32_t> const &inside, vec2<uint16_t> const &outside_uv, vec2<uint16_t> const &inside_uv) {
  auto const result{intersect<2>(outside,inside,{outside_uv.x,outside_uv.y},{inside_uv.x,inside_uv.y})};
  return {
    .position{vec2<int32_t>{result.position}},
    .uv_q7{result.attributes[0],result.attributes[1]},
  };
}

vec2<int32_t> intersect_near_plane(vec3<int32_t> const &outside, vec3<int32_t> const &inside) {
  return vec2<int32_t>{intersect<0>(outside,inside,{},{}).position};
}

vec2<int16_t> project_near_plane(vec2<int32_t> const &point, aperture const opening, vec2<int16_t> const &origin) {
  unsigned int const shift{projection_shift(opening)};
  int32_t constexpr normal_coordinate_limit{2030}, screen_saturation{32000};
  int32_t const limit{normal_coordinate_limit * static_cast<int32_t>(1u << (shift - 8)) * 65536};
  auto const coordinate{[&](int32_t const value, int16_t const offset){
    // Saturation writes a final screen word, without adding the origin. The
    // ordinary path retains arithmetic-shift rounding even for negative XY.
    if(value > limit) return static_cast<int16_t>(screen_saturation);
    if(value < -limit) return static_cast<int16_t>(-screen_saturation);
    return word((value >> (shift + 4)) + offset);
  }};
  return {coordinate(point.x,origin.x),coordinate(point.y,origin.y)};
}

} // namespace tfx::render
