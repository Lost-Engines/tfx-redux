#include "scene_display.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>
#include "render/sprites.h"

namespace tfx::cockpit {

render::camera scene_camera_request::view() const {
  vec2d const centre{vec2d{minimum + (maximum - minimum) / 2}};
  auto result{render::orbit_camera(angles, 0, centre)};
  result.position = vec3d{position};
  result.world_anchor = vec3<int32_t>{position};
  result.viewport_minimum = vec2<int16_t>{minimum};
  result.viewport_maximum = vec2<int16_t>{maximum};
  return result;
}
scene_displays::scene_displays(instrument_assets const &source)
  : assets{source},
    animation{
  .reticle_countdown{assets.camera_blink},
  .orbit_angles{vec3<math::angle>{assets.camera_orbit_angles}},
  .orbit_step{assets.camera_orbit_step},
} {
}
scene_camera_animation const &scene_displays::state() const {
  return animation;
}
void scene_displays::draw(render::software_renderer &renderer, display_side const side, display_page const page, vec2<int16_t> const &offset, scene_camera_state &state, scene_painter const &scene, render::framebuffer *const coverage) {
  unsigned int const slot{side == display_side::left ? 0u : 2u};
  auto const point{[&](vec2<int> const &local){
    return vec2<int>{math::signed_word(static_cast<uint16_t>(assets.display_origins[slot].x + offset.x + local.x)), math::signed_word(static_cast<uint16_t>(assets.display_origins[slot].y + offset.y + local.y))};
  }};
  scene_camera_request request{
    .position{vec3<int32_t>{state.position}},
    .angles{0, 0, 0},
    .minimum{point({0, 0})},
    .maximum{point(vec2<int>{assets.display_sizes[slot]})},
    .tracked_position{},
  };
  std::optional<long> range;
  if(page == display_page::camera) {
    // Native 0x88378 advances its two-draw reticle phase even for an unlocked FLIR view.
    if(!animation.reticle_countdown) animation.reticle_countdown = 2;
    animation.reticle_countdown = math::signed_word(static_cast<uint16_t>(animation.reticle_countdown - 1));
    if(state.tracked_position_q8) {
      auto const &target{*state.tracked_position_q8};
      vec3<int32_t> const position{target.x >> 8, target.y >> 8, target.z >> 8};
      auto const bearing{math::bearing(state.position, position)};
      request.angles = {static_cast<uint16_t>(-bearing.x), static_cast<uint16_t>(-bearing.y), static_cast<uint16_t>(-state.tracking_roll)};
      request.tracked_position.emplace(vec3<int32_t>{position});
    } else {
      if(state.aircraft == flight::player_aircraft::nighthawk) {
        // The F-117's free FLIR gimbal is limited to ±45 degrees from the aircraft.
        int constexpr gimbal_limit{0x2000};
        for(unsigned int axis{0}; axis != 2; ++axis) {
          int const delta{math::signed_word(static_cast<uint16_t>(state.free_angles[axis] - state.angles[axis]))};
          state.free_angles[axis] = static_cast<uint16_t>(state.angles[axis] + std::clamp(delta, -gimbal_limit, gimbal_limit));
        }
      }
      request.angles = {static_cast<uint16_t>(-state.free_angles.x), static_cast<uint16_t>(-state.free_angles.y), static_cast<uint16_t>(-state.angles.z)};
    }
  } else if(page == display_page::ground_database) {
    request.position = vec3<int32_t>{state.ground_target};
    animation.orbit_angles.x = 0x2000;
    int constexpr target_orbit_distance{-1700};
    auto const &height_bounds{state.height_bounds ? *state.height_bounds : assets.camera_height_bounds};
    math::advance_position(request.position, animation.orbit_angles, target_orbit_distance, height_bounds.x, height_bounds.y);
    request.angles = vec3<math::angle>{static_cast<uint16_t>(-animation.orbit_angles.x), static_cast<uint16_t>(-animation.orbit_angles.y), static_cast<uint16_t>(-animation.orbit_angles.z)};
    animation.orbit_angles.y = static_cast<uint16_t>(animation.orbit_angles.y + animation.orbit_step);
    auto const magnitude{[](int32_t const a, int32_t const b){
      uint32_t const value{static_cast<uint32_t>(a) - static_cast<uint32_t>(b)};
      return std::bit_cast<int32_t>(std::bit_cast<int32_t>(value) < 0 ? 0u - value : value);
    }};
    int constexpr world_units_per_display_mile{682};
    range = std::max(magnitude(state.ground_target.x, state.position.x), magnitude(state.ground_target.z, state.position.z)) / world_units_per_display_mile;
  } else {
    throw std::runtime_error{"invalid scene-camera display page"};
  }
  auto const projected{scene(request)};
  if(request.tracked_position) {
    auto const mask{animation.reticle_countdown ? instrument_mask::target_reticle : instrument_mask::alternate_target_reticle};
    render::draw_monochrome(renderer.output, assets.masks[std::to_underlying(mask)], {static_cast<uint16_t>(projected.x - 4), static_cast<uint16_t>(projected.y - 4)}, 15, coverage, renderer.clipping);
  }
  if(range) {
    std::array<char, 64> text{};
    int const length{std::snprintf(text.data(), text.size(), assets.ground_range_format.c_str(), *range)};
    if(length < 0 || static_cast<unsigned int>(length) >= text.size()) throw std::runtime_error{"ground-target range exceeds its display buffer"};
    auto const position{point({4, 30})};
    renderer.text(position.x, position.y, {text.data(), static_cast<unsigned int>(length)}, 15, coverage);
  }
}

} // namespace tfx::cockpit
