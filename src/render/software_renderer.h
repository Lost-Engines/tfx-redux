#pragma once

#include <span>
#include <string_view>
#include <vector>
#include "models/shape.h"
#include "render/camera.h"
#include "render/clipping.h"
#include "render/colour_remap.h"
#include "render/framebuffer.h"
#include "render/point_visibility.h"
#include "render/texture_overlay.h"
#include "resources/executable_tables.h"

namespace tfx::render {

struct screen_vertex {
  vec2d position;
  vec2d uv;
  double shade;
};

std::array<vec2<int32_t>, 8> rounded_corner(vec2<int32_t> const &previous, vec2<int32_t> const &corner, vec2<int32_t> const &next);

class software_renderer {
private:
  resources::executable_tables const &tables;
  uint16_t procedural_seed{0x4321};
  // Reused within each synchronous draw; no allocation per polygon/scanline.
  std::array<std::vector<models::model_vertex>,2> clip_vertices;
  std::vector<screen_vertex> projected_vertices, scanline_hits;
  struct point_sprite { vec2<int> position; vec2d ray; double depth, diameter; uint8_t colour; int mode; };
  point_visibility point_depth;
  std::vector<point_sprite> points;
  bool collect_points{false};
  void point_disc(point_sprite const &point);
  void ink(int x, int y, uint8_t colour, int mode);
  void patterned_span(int y, double left, double right, uint8_t colour, int mode, int bottom, bool inclusive, bool paint = true);

public:
  framebuffer output;
  raster_clip clipping;
  software_renderer(resources::executable_tables const &tables, uint16_t procedural_seed = 0x4321);
  uint16_t procedural_state() const;
  void begin_points();
  void finish_points();
  void background(camera const &view, uint8_t sky, uint8_t ground);
  unsigned int text_width(std::string_view message) const;
  void text(int x, int y, std::string_view message, uint8_t colour, framebuffer *coverage = nullptr);
  // Optional opaque, unlit instrument overlay. Direct canvas mappings sample
  // fractional UVs once; ordinary texture artwork retains its original UVs.
  void polygon(std::span<screen_vertex const> vertices, uint8_t colour, int mode = -1, std::span<uint8_t const> texture = {}, colour_map const *remap = nullptr, texture_overlay overlay = {});
  void circle(int x, int y, unsigned int radius, uint8_t colour, int mode = -1);
  void line(vec2d const &a, vec2d const &b, uint8_t colour);
  void primitive(models::primitive const &source, std::span<models::model_vertex const> camera_vertices, camera const &view, std::span<uint8_t const> texture, colour_map const *remap = nullptr, texture_overlay overlay = {});
};

} // namespace tfx::render
