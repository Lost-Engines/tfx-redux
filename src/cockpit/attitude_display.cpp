#include "attitude_display.h"
#include "vector_hud.h"
#include <numbers>
#include <algorithm>
#include <cmath>
#include "presentation/drawing.h"
#include "render/sprites.h"

namespace tfx::cockpit {
namespace {

int word(int const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t wrapped_sum(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

} // anonymous namespace

attitude_display::attitude_display(instrument_assets const &source, resources::archive const &game)
  : assets{source}, heading_strip{resources::decode_sprites(game.read(assets.heading_bank)).at(0)}, numbers{resources::decode_sprites(game.read(assets.pitch_bank))} {
  // Native 0x809e4: ten-degree steps use 1820 angle units, with three
  // wraparound entries on either side of the 36-point circle.
  unsigned int constexpr step{1820}, circle_steps{36}, padding{3};
  for(unsigned int index{0}; index != circle_steps; ++index) arc[index + padding] = math::rotate_words({4096,0},static_cast<math::angle>(index * step));
  for(unsigned int index{0}; index != padding; ++index) {
    arc[index] = arc[index + circle_steps];
    arc[index + circle_steps + padding] = arc[index + padding];
  }
}
vec2<int> attitude_display::draw_heading(render::software_renderer &renderer, attitude_display_state const &state, render::framebuffer *const coverage) const {
  // Native 0x80ffc, as called by the forward-HUD dispatcher. The scale
  // deliberately uses 183 units per degree and complements -heading.
  unsigned int const degrees{static_cast<uint16_t>(state.angles.y - 1) / 183u};
  int const displacement{degrees <= 180 ? -static_cast<int>(degrees / 2) : 180 - static_cast<int>(degrees / 2)};
  vec2<int> const low{std::max(0,word(128 + state.offset.x)),std::max(0,word(1 + state.offset.y))};
  vec2<int> const high{std::min(319,word(200 + state.offset.x)),std::min(199,word(86 + state.offset.y))};
  renderer.clipping = render::raster_clip::viewport(low,high);
  if(renderer.output.hud_capture) {
    double degrees{state.angles.y*360.0/65536};if(degrees>180) degrees-=360;
    renderer.output.hud_capture->sprite(heading_strip,{160-degrees/2+state.offset.x,6.0+state.offset.y},renderer.clipping);
  } else render::draw_sprite(renderer.output,heading_strip,{static_cast<uint16_t>(160 + displacement + state.offset.x),static_cast<uint16_t>(6 + state.offset.y)},coverage,renderer.clipping);
  return {std::min(319,word(214 + state.offset.x)),std::min(199,word(77 + state.offset.y))};
}
void attitude_display::draw_pitch(render::software_renderer &renderer, attitude_display_state const &state, render::framebuffer *const coverage, vec2<int> const &line_limit) const {
  // Native 0x80ab8 and the separate sky/HUD projection setter 0xc82b9.
  // This two-angle matrix is distinct from the model renderer's view matrix.
  auto const &table{math::sine_table()};
  auto const sine{[&](int const phase) { return static_cast<int32_t>(table[static_cast<unsigned int>(phase) & (math::angle_steps - 1)]); }};
  auto const cosine{[&](int const phase) { return sine(phase + math::angle_steps / 4); }};
  int const pitch{state.projection_angles.x >> 6}, roll{state.projection_angles.y >> 6};
  vec2<int32_t> const depth{-sine(pitch),cosine(pitch)};
  vec2<int32_t> const horizontal{(sine(pitch + roll) >> 1) - (sine(pitch - roll) >> 1),(cosine(pitch - roll) >> 1) - (cosine(pitch + roll) >> 1)};
  vec2<int32_t> const vertical{(cosine(pitch + roll) >> 1) + (cosine(pitch - roll) >> 1),(sine(pitch + roll) >> 1) + (sine(pitch - roll) >> 1)};
  vec2<int> const low{std::max(0,word(100 + state.offset.x)),std::max(0,word(15 + state.offset.y))};
  vec2<int> const high{std::min(line_limit.x,word(220 + state.offset.x)),std::min(line_limit.y,word(120 + state.offset.y))};
  renderer.clipping = render::raster_clip::viewport(low,high);
  unsigned int const first{(state.angles.x & 0xffc0u) / 1820 + 1};
  uint8_t constexpr colour{6};
  for(unsigned int ordinal{0}; ordinal != 5; ++ordinal) {
    unsigned int const index{first + ordinal};
    auto const &mark{assets.pitch_marks.at(index)};
    auto const &point{arc.at(index)};
    int32_t const divisor{std::max(16,wrapped_sum(point.y * depth.x,point.x * depth.y) >> 16)};
    auto const project{[&](vec2<int32_t> const &axis, int const centre) {
      int32_t const numerator{wrapped_sum(point.y * axis.x,point.x * axis.y) >> 8};
      double constexpr original_focal_distance{render::projection_focal_length(render::aperture::normal)};
      int const displacement{state.focal_distance == original_focal_distance ? numerator / divisor : static_cast<int>(std::lround(numerator / static_cast<double>(divisor) * state.focal_distance / original_focal_distance))};
      return word(std::clamp(displacement,-32000,32000) + centre);
    }};
    vec2<int> const origin{word(project(horizontal,state.projection_origin.x) + state.offset.x),project(vertical,state.projection_origin.y)};
    auto const rotate{[&](vec2<int16_t> const &vertex) {
      auto const rotated{math::rotate_words(vertex,state.angles.z)};
      return vec2<int>{word(rotated.x + origin.x),word(rotated.y + origin.y)};
    }};
    if(renderer.output.hud_capture) {
      // Keep subpixel positions and bank rotation until final rasterisation.
      auto const project_precise{[&](vec2<int32_t> const &axis,int centre) {
        double const numerator{wrapped_sum(point.y*axis.x,point.x*axis.y)/256.0};
        return std::clamp(numerator/divisor*state.focal_distance/render::projection_focal_length(render::aperture::normal),-32000.0,32000.0)+centre;
      }};
      vec2d const centre{project_precise(horizontal,state.projection_origin.x)+state.offset.x,project_precise(vertical,state.projection_origin.y)};
      double const angle{state.angles.z*2*std::numbers::pi/65536};
      auto const rotate_precise{[&](vec2<int16_t> const &v) {
        return centre+vec2d{v.x*std::cos(angle)-v.y*std::sin(angle),v.x*std::sin(angle)+v.y*std::cos(angle)};
      }};
      for(unsigned side=0;side<2;++side) for(unsigned segment=0;segment<2;++segment) {
        auto const start{side*3+segment};
        renderer.output.hud_capture->line(rotate_precise(mark.vertices[start]),rotate_precise(mark.vertices[start+1]),low,high,mark.dashed ? 0xaa : 255);
      }
      if(mark.label!=-1) for(int side : {-35,38}) renderer.output.hud_capture->sprite(numbers.at(static_cast<unsigned>(mark.label)),rotate_precise({static_cast<int16_t>(side),0}),renderer.clipping);
      continue;
    }
    for(unsigned int side{0}; side != 2; ++side) for(unsigned int segment{0}; segment != 2; ++segment) {
      unsigned int const start{side * 3 + segment};
      auto const from{rotate(mark.vertices[start])}, to{rotate(mark.vertices[start + 1])};
      auto const paint{[&](render::framebuffer &target, uint8_t const ink) {
        if(mark.dashed) presentation::draw_patterned_line(target,vec2<int>{from},vec2<int>{to},low,high,ink,0xaa);
        else presentation::draw_clipped_line(target,vec2<int>{from},vec2<int>{to},low,high,ink);
      }};
      paint(renderer.output,colour);
      if(coverage) paint(*coverage,255);
    }
    if(mark.label != -1) for(int const side : {-35,38}) {
      auto const centre{rotate({static_cast<int16_t>(side),0})};
      render::draw_sprite(renderer.output,numbers.at(static_cast<unsigned int>(mark.label)),{static_cast<uint16_t>(centre.x),static_cast<uint16_t>(centre.y)},coverage,renderer.clipping);
    }
  }
}

} // namespace tfx::cockpit
