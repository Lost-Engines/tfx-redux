#pragma once

#include "math/attitude.h"
#include "cockpit/instrument_assets.h"
#include "render/software_renderer.h"

namespace tfx::cockpit {

enum class target_sight { normal, compact };
struct air_target_reading {
  std::string_view name;
  vec3<int32_t> position_q8{0,0,0};
  math::angle heading{0};
  int32_t speed_q16{0};
};
struct target_display_state {
  vec3<int32_t> player{0,0,0};
  math::attitude orientation;
  vec2<int16_t> projected{0,0};
  vec2<int32_t> tracking_q8{0,0};
  target_sight sight{target_sight::normal};
  vec2<int16_t> offset{0,0};
};
class target_display {
private:
  instrument_assets const &assets;
  void direction_marker(render::software_renderer &renderer, vec3<int32_t> const &delta, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *coverage) const;

public:
  explicit target_display(instrument_assets const &assets);
  void heading(render::software_renderer &renderer, math::angle target, math::angle player, vec2<int16_t> const &offset, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *coverage = nullptr) const;
  void information(render::software_renderer &renderer, air_target_reading const &target, target_display_state const &state, render::framebuffer *coverage = nullptr) const;
  void direction(render::software_renderer &renderer, air_target_reading const &target, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *coverage = nullptr) const;
  void ground(render::software_renderer &renderer, vec3<int32_t> const &target, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *coverage = nullptr) const;
  void edge_marker(render::software_renderer &renderer, target_display_state const &state, render::framebuffer *coverage = nullptr) const;
  void marker(render::software_renderer &renderer, target_sight sight, bool acquired, vec2<int> const &centre, render::framebuffer *coverage = nullptr) const;
};

} // namespace tfx::cockpit
