#include "glare.h"
#include <algorithm>
#include <cstdlib>
#include "render/palette_lighting.h"

namespace tfx::render {

uint8_t glare_level(point_projection const &projection, glare_context const &context) {
  uint8_t constexpr flak_flash_level{40};
  int16_t constexpr sun_direction_offset{500};
  if(context.flak_flash) return flak_flash_level;
  if((context.cloud_layer && context.layer_y <= context.camera_anchor.y) || context.view == cockpit::view_mode::target) return lighting_stages;
  // The original projects a fixed world direction relative to its integer
  // camera anchor. Each source coordinate is stored/sign-extended as a word.
  vec3<int32_t> const sun{
    math::signed_word(static_cast<uint16_t>(context.camera_anchor.x)),
    math::signed_word(static_cast<uint16_t>(static_cast<uint32_t>(context.camera_anchor.y) - sun_direction_offset)),
    math::signed_word(static_cast<uint16_t>(static_cast<uint32_t>(context.camera_anchor.z) - sun_direction_offset)),
  };
  auto const point{projection.integer(sun)};
  if(!point || point->x < context.minimum.x || point->x > context.maximum.x || point->y < context.minimum.y || point->y > context.maximum.y) return lighting_stages;
  auto const difference{vec2<int32_t>{projection.origin} - vec2<int32_t>{*point}};
  // ABS returns a dword, but the original stores each result as a signed word
  // before its max-plus-half-min distance and final arithmetic word shift.
  int16_t const x{math::signed_word(static_cast<uint16_t>(std::abs(difference.x)))};
  int16_t const y{math::signed_word(static_cast<uint16_t>(std::abs(difference.y)))};
  int16_t const distance{math::signed_word(static_cast<uint16_t>(std::max(x, y) + (std::min(x, y) >> 1)))};
  int const level{distance >> 1};
  return level < static_cast<int>(lighting_stages) ? static_cast<uint8_t>(level + 1) : lighting_stages;
}

} // namespace tfx::render
