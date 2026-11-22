#pragma once

#include <functional>
#include "cockpit/mfd.h"

namespace tfx::cockpit {

struct scene_camera_state {
  flight::player_aircraft aircraft{flight::player_aircraft::eurofighter};
  vec3<int32_t> position{0,0,0};
  vec3<math::angle> angles{0,0,0};
  vec2<math::angle> free_angles{0,0};
  std::optional<vec3<int32_t>> tracked_position_q8;
  math::angle tracking_roll{0};                                                // The native bearing helper leaves this caller-local word unwritten.
  vec3<int32_t> ground_target{0,0,0};
  std::optional<vec2<int32_t>> height_bounds{};                                // Mission-initialized limits; absent for prelaunch inspection.
};
struct scene_camera_request {
  vec3<int32_t> position;
  vec3<math::angle> angles;                                                    // Native view-setter angles, rather than aircraft Euler angles.
  vec2<int> minimum;
  vec2<int> maximum;
  std::optional<vec3<int32_t>> tracked_position;
  render::camera view() const;
};
struct scene_camera_animation {
  int16_t reticle_countdown;
  vec3<math::angle> orbit_angles;
  int16_t orbit_step;
};
using scene_painter = std::function<vec2<int16_t>(scene_camera_request const&)>;

class scene_displays {
private:
  instrument_assets const &assets;
  scene_camera_animation animation;

public:
  explicit scene_displays(instrument_assets const &assets);
  scene_camera_animation const &state() const;
  void draw(render::software_renderer &renderer, display_side side, display_page page, vec2<int16_t> const &offset, scene_camera_state &state, scene_painter const &scene, render::framebuffer *coverage = nullptr);
};

} // namespace tfx::cockpit
