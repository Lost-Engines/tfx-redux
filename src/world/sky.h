#pragma once

#include <optional>
#include <string_view>
#include "render/point_projection.h"

namespace tfx::world {

int16_t constexpr sky_visibility_radius{4000};
enum class sky_model {
  sun,
  lightning
};
std::string_view sky_model_name(sky_model model);
struct sky_request {
  sky_model model;
  vec3<int16_t> position;
};
struct sky_state {
  int16_t delay{5};
  uint16_t phase{0xac57};
  uint16_t scatter{0x6b5a};
  vec2<int32_t> lightning_position{0, 0};
  uint8_t pending_flash{0};
};
struct sky_context {
  bool suppressed{false};                                                      // Native environment bit 0x200 (twilight).
  bool cloud_layer{false};
  bool storm{false};
  bool animate{false};
  int16_t layer_y{0};
  vec3<int32_t> player;
  vec3<int32_t> camera_anchor;
};
// Native 0xa26cc / 0xa25b4. The flak controller has a separate owner.
std::optional<sky_request> advance_sky(sky_state &state, sky_context const &context);
// Native 0x73e9a: a pending strike flashes once it projects inside the viewport.
bool consume_lightning_flash(sky_state &state, render::point_projection const &projection);
struct terrain_layer {
  bool upper;
  int16_t height;
};
// Native frame selection at 0x72149; equality belongs to the upper grid.
terrain_layer select_terrain_layer(bool cloud_layer, int16_t layer_y, int16_t offset_y, int32_t camera_y);

} // namespace tfx::world
