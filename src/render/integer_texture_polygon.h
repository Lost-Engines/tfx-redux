#pragma once

#include "render/colour_remap.h"
#include "render/framebuffer.h"

namespace tfx::render {

struct textured_vertex {
  vec2<int16_t> position;
  vec2<uint16_t> uv_q7;
};

// Original affine texture path: signed-word clipping, Q7 atlas coordinates,
// integer edges and Q24 pixel stepping, followed by palette transparency.
void integer_texture_polygon(framebuffer &output, std::span<textured_vertex const> vertices, std::span<uint8_t const> texture, colour_map const &remap, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum);

} // namespace tfx::render
