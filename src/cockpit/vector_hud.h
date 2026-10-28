#pragma once

#include "cockpit/instrument_assets.h"
#include "render/clipping.h"
#include "render/camera.h"

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
  struct glass_projection {
    render::camera view;
    mat3d rotation;
    std::array<vec3d,6> edges;
  };
  std::optional<glass_projection> glass;
  vec3d glass_ray(vec2d const &point) const;
  bool on_glass(vec2d const &pixel) const;
  void stroke(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, double width);
  void bitmap(std::span<uint8_t const> pixels, vec2<int> const &dimensions, vec2d const &origin, render::raster_clip const &clip);
public:
  explicit vector_hud(instrument_assets const &assets) : assets{assets} {}
  // Shared geometry of the retail cpt_*/cpt*_f HUD frames. The source HUD
  // spans x=80..240, y=0..97. At the normal eye position (z=-.25), the
  // glass covers slopes y=-.173..+.058 and x=+/- .18.. .20. Fit that angular
  // window with a small border; the optical axis falls below its centre.
  // Aiming projection must use this same origin/focal length, so resizing the
  // layout never moves the horizon or world-space target cues off their rays.
  static constexpr double glass_focal_distance{450};
  static constexpr vec2d glass_origin{160,76};
  static constexpr std::array<vec3d,6> glass_vertices{{
    {-.1375,-.13,.5025},{.1375,-.13,.5025},{.15,-.12,.515},
    {.1825,.0525,.6625},{-.1825,.0525,.6625},{-.15,-.12,.515},
  }};
  void begin(vec2<int> const &dimensions);
  void attach_to_glass(render::camera const &view, vec3d const &model_position, mat3d const &rotation);
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
