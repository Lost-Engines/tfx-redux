#pragma once

#include "render/colour_remap.h"
#include "render/framebuffer.h"

namespace tfx::render {

struct indexed_vertex {
  vec2<int16_t> position;
  uint16_t shade_q8;
};

enum class polygon_facing { cull, unchecked };

struct polygon_span {
  int16_t row;
  int16_t left;
  int16_t right;
};
// Ascending native scanlines, including empty spans so bottom-relative drawing
// phases remain intact. Consumers can traverse backwards like the VGA painter.
std::vector<polygon_span> integer_polygon_spans(std::span<vec2<int16_t> const> vertices, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum, polygon_facing facing = polygon_facing::cull);

// Original Gouraud path: interpolate authored 8.8 palette indices, then remap.
void integer_gouraud_polygon(framebuffer &output, std::span<indexed_vertex const> vertices, colour_map const &remap, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum);

// Original solid 2D convex polygon path: signed-word clipping and Q16 edges.
void integer_polygon(framebuffer &output, std::span<vec2<int16_t> const> vertices, uint8_t colour, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum);

} // namespace tfx::render
