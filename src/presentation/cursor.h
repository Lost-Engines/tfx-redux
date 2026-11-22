#pragma once

#include "resources/media.h"

namespace tfx::presentation {
struct pointer_shape {
  resources::indexed_image image;
  vec2<int16_t> adjustment;
};
struct pointer_style {
  pointer_shape idle;
  pointer_shape active;
};
std::array<pointer_style, 2> load_pointers(std::span<uint8_t const> executable);
void draw_pointer(render::framebuffer &target, pointer_shape const &shape, vec2<int16_t> const &position);
} // namespace tfx::presentation
