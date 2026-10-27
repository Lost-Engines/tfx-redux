#pragma once

#include "cockpit/instrument_assets.h"
#include "render/clipping.h"

namespace tfx::cockpit {

// Optical HUD layer, in the original instrument coordinates. Geometry is
// rasterised only after projection to the actual framebuffer, with a union
// coverage mask so overlapping strokes do not become brighter at joins.
class vector_hud {
  instrument_assets const &assets;
  vec2<int> size{0,0};
  vec2d offset{0,0};
  double scale{1};
  std::vector<uint8_t> ink;
  bool painted{false};
  void stroke(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, double width);
  void bitmap(std::span<uint8_t const> pixels, vec2<int> const &dimensions, vec2d const &origin, render::raster_clip const &clip);
public:
  explicit vector_hud(instrument_assets const &assets) : assets{assets} {}
  void begin(vec2<int> const &dimensions);
  void line(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, uint8_t pattern = 255);
  void circle(vec2d const &centre, double radius, render::raster_clip const &clip, bool dotted = false);
  void pixel(vec2d const &point, render::raster_clip const &clip = {});
  void glyph(std::span<uint8_t const> rows, vec2d const &origin, render::raster_clip const &clip);
  void sprite(resources::indexed_image const &image, vec2d const &centre, render::raster_clip const &clip);
  void mask(resources::monochrome_image const &image, vec2<int> const &origin, render::raster_clip const &clip);
  void compose(render::framebuffer &target, std::span<uint8_t const> occlusion) const;
};

class capture_hud {
  render::framebuffer &image;
  vector_hud *previous;
public:
  capture_hud(render::framebuffer &image, vector_hud *sink) : image{image}, previous{image.hud_capture} { image.hud_capture=sink; }
  ~capture_hud() { image.hud_capture=previous; }
  capture_hud(capture_hud const&)=delete;
  capture_hud &operator=(capture_hud const&)=delete;
};

} // namespace tfx::cockpit
