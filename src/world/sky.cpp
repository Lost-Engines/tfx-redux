#include "sky.h"
#include <bit>
#include <stdexcept>

namespace tfx::world {
namespace {
int32_t add(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
int16_t word(int32_t const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
}

std::string_view sky_model_name(sky_model const model) {
  switch(model) {
  case sky_model::sun:
    return "sun1";
  case sky_model::lightning:
    return "lightnin";
  }
  throw std::invalid_argument{"unknown sky model"};
}
std::optional<sky_request> advance_sky(sky_state &state, sky_context const &context) {
  if(context.suppressed) return std::nullopt;
  if(context.cloud_layer && context.layer_y <= context.player.y) {
    if(!context.storm || !context.animate) return std::nullopt;
    // Unlike the flak timer, a zero countdown wraps rather than firing.
    state.delay = word(state.delay - 1);
    if(state.delay) return std::nullopt;
    uint16_t const phase{state.phase};
    state.delay = phase & 127;
    state.phase = static_cast<uint16_t>(phase + state.scatter);
    state.scatter = static_cast<uint16_t>(state.scatter + (math::signed_word(phase) >> 1) + (static_cast<uint32_t>(phase) << 15));
    auto const offset{math::rotate_words({0, math::signed_word(state.scatter)}, phase)};
    state.lightning_position = {add(context.player.x, offset.x * 4), add(context.player.z, offset.y * 4)};
    state.pending_flash = 2;
    return sky_request{
      .model{sky_model::lightning},
      .position{word(state.lightning_position.x), 0, word(state.lightning_position.y)},
    };
  }
  int32_t constexpr sun_offset{500};
  return sky_request{
    .model{sky_model::sun},
    .position{word(context.camera_anchor.x), word(add(context.camera_anchor.y, -sun_offset)), word(add(context.camera_anchor.z, -sun_offset))},
  };
}
bool consume_lightning_flash(sky_state &state, render::point_projection const &projection) {
  if(!state.pending_flash) return false;
  auto const point{projection.integer({word(state.lightning_position.x), -100, word(state.lightning_position.y)})};
  if(!point || point->x <= 0 || point->x >= render::screen_width || point->y <= 0 || point->y >= render::screen_height - 1 || state.pending_flash != 2) return false;
  state.pending_flash = 0;
  return true;
}
terrain_layer select_terrain_layer(bool const cloud_layer, int16_t const layer_y, int16_t const offset_y, int32_t const camera_y) {
  bool const upper{cloud_layer && layer_y >= camera_y};
  return {
    .upper{upper},
    .height{upper ? word(static_cast<int32_t>(layer_y) + offset_y) : int16_t{0}},
  };
}

} // namespace tfx::world
