#include "designator.h"
#include "render/continuous_pose.h"
#include "render/sprites.h"
#include <bit>

namespace tfx::cockpit {

view_pose designator_pose(sensors::designator const &state) {
  vec3<math::angle> const angles{static_cast<math::angle>(-state.pitch),static_cast<math::angle>(-state.heading),0};
  view_pose result{
    .horizon_angles{vec2<math::angle>{angles.x,0}},
    .horizon_camera_y{math::signed_word(static_cast<uint16_t>(state.camera.y))},
    .anchor{vec3<int32_t>{state.camera}},
    .angles{vec3<math::angle>{angles}},
    .matrix{math::view_rotation_words(angles)},
  };
  // B9A6C rotates the full integer camera dwords rather than shifting: bits
  // above the word boundary survive in the fractional half of the cache.
  for(unsigned int axis{0}; axis != 3; ++axis) result.position_q16[axis] = std::bit_cast<int32_t>(std::rotr(static_cast<uint32_t>(state.camera[axis]),16));
  return result;
}

render::camera designator_display_camera(sensors::designator const &state,vec2d const &projection_origin) {
  auto const original{designator_pose(state).camera(projection_origin)};
  return render::continuous_camera(original,render::continuous_rotation(vec3<math::angle>{state.pitch,state.heading,0}));
}

designator_display::designator_display(instrument_assets const &source, resources::archive const &game, designator_animation const initial)
  : assets{source}, marker{resources::decode_sprites(game.read(assets.designator_bank)).at(0)}, animation{initial} {
}
designator_animation const &designator_display::state() const { return animation; }
void designator_display::draw(render::software_renderer &renderer, vec2<int16_t> const &point, bool const tracking, render::framebuffer *const coverage) {
  // Search and tracking retain independent two-draw phases (884A/8848).
  auto &phase{tracking ? animation.tracking : animation.search};
  if(!phase) phase = 2;
  phase = math::signed_word(static_cast<uint16_t>(phase - 1));
  renderer.clipping = render::raster_clip::viewport({0,0},{render::screen_width - 1,render::screen_height - 1});
  // The controller zero-extends projected words for the sprite call, while
  // label offsets below sign-extend their resulting words. A failed (-1,-1)
  // projection therefore hides the marker rather than clipping it at the corner.
  render::draw_sprite(renderer.output,marker,{static_cast<uint16_t>(point.x),static_cast<uint16_t>(point.y)},coverage,renderer.clipping);
  int constexpr label_colour{16},label_y_offset{-10},target_x_offset{-21},laser_x_offset{17};
  auto const label{[&](unsigned int const index, int const x_offset) {
    renderer.text(math::signed_word(static_cast<uint16_t>(point.x + x_offset)),math::signed_word(static_cast<uint16_t>(point.y + label_y_offset)),assets.designator_labels[index],label_colour,coverage);
  }};
  if(tracking || !phase) label(0,target_x_offset);
  if(!tracking || !phase) label(1,laser_x_offset);
}

} // namespace tfx::cockpit
