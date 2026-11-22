#pragma once

#include "render/framebuffer.h"

namespace tfx::render {

enum class marker_kind {
  pixel,
  block
};
void draw_marker(framebuffer &target, vec2<uint16_t> const &position, uint8_t colour, marker_kind kind = marker_kind::pixel, framebuffer *coverage = nullptr);

} // namespace tfx::render
