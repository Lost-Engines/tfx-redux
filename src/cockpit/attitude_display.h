#pragma once

#include "cockpit/instrument_assets.h"
#include "render/software_renderer.h"
#include "resources/archive.h"

namespace tfx::cockpit {

struct attitude_display_state {
  vec3<math::angle> angles{0,0,0};
  vec2<math::angle> projection_angles{0,0};
  vec2<int16_t> projection_origin{159,56};
  vec2<int16_t> offset{0,0};
  double focal_distance{render::projection_focal_length(render::aperture::normal)};
};
class attitude_display {
private:
  instrument_assets const &assets;
  resources::indexed_image heading_strip;
  std::vector<resources::indexed_image> numbers;
  std::array<vec2<int16_t>,42> arc;

public:
  attitude_display(instrument_assets const &assets, resources::archive const &game);
  vec2<int> draw_heading(render::software_renderer &renderer, attitude_display_state const &state, render::framebuffer *coverage = nullptr) const;
  void draw_pitch(render::software_renderer &renderer, attitude_display_state const &state, render::framebuffer *coverage = nullptr, vec2<int> const &line_limit = {319,199}) const;
};

} // namespace tfx::cockpit
