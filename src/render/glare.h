#pragma once

#include "cockpit/view_control.h"
#include "render/framebuffer.h"
#include "render/point_projection.h"

namespace tfx::render {

struct glare_context {
  vec3<int32_t> camera_anchor;
  vec2<int16_t> minimum{0, 0};
  vec2<int16_t> maximum{screen_width - 1, screen_height - 1};
  cockpit::view_mode view{cockpit::view_mode::cockpit};
  bool cloud_layer{false};
  int16_t layer_y{0};
  bool flak_flash{false};
};

// Native 0x73838 supplies the white-tint level; the palette painter is separate.
uint8_t glare_level(point_projection const &projection, glare_context const &context);

} // namespace tfx::render
