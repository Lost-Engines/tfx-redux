#include "target_display.h"
#include <algorithm>
#include <utility>
#include "cockpit/format.h"
#include "presentation/drawing.h"
#include "render/sprites.h"

namespace tfx::cockpit {
namespace {

int word(int const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t difference(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t magnitude(int32_t const value) { return value < 0 ? difference(0,value) : value; }
vec2<int> shifted(vec2<int> const &point, vec2<int16_t> const &offset) { return {word(point.x + offset.x),word(point.y + offset.y)}; }

} // anonymous namespace

target_display::target_display(instrument_assets const &source) : assets{source} {
}
void target_display::heading(render::software_renderer &renderer, math::angle const target, math::angle const player, vec2<int16_t> const &offset, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *const coverage) const {
  // Native 0x7e4e0 rotates the authored two-segment heading chevron.
  std::array<vec2<int>,3> vertices;
  for(unsigned int index{0}; index != vertices.size(); ++index) {
    auto const rotated{math::rotate_words(assets.target_heading_shape[index],static_cast<math::angle>(target - player))};
    vertices[index] = shifted({160 + rotated.x,56 + rotated.y},offset);
  }
  for(unsigned int index{0}; index != 2; ++index) {
    presentation::draw_clipped_line(renderer.output,vec2<int>{vertices[index]},vec2<int>{vertices[index + 1]},minimum,maximum,6);
    if(coverage) presentation::draw_clipped_line(*coverage,vec2<int>{vertices[index]},vec2<int>{vertices[index + 1]},minimum,maximum,255);
  }
}
void target_display::information(render::software_renderer &renderer, air_target_reading const &target, target_display_state const &state, render::framebuffer *const coverage) const {
  // Native 0x7f59c/0x7ddb0: the uppercased first word, then four data rows.
  std::string name{target.name.substr(0,std::min(target.name.find(' '),target.name.find('\0')))};
  for(char &character : name) if(character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
  auto const text{[&](int const row, std::string const &message) {
    auto const point{shifted({96,row},state.offset)};renderer.text(point.x,point.y,message,6,coverage);
  }};
  text(2,name);
  int16_t const speed{static_cast<int16_t>(target.speed_q16 >> 16)};
  text(8,detail::decimal(assets.target_formats[0],static_cast<double>(static_cast<long double>(speed) * assets.speed_scale),0));
  unsigned long const heading{static_cast<unsigned int>(target.heading) * 360u >> 16};
  text(14,detail::formatted(assets.target_formats[1],heading));
  unsigned int constexpr feet_per_world_unit{8};
  unsigned int const altitude{(0u - static_cast<uint32_t>(target.position_q8.y >> 8)) * feet_per_world_unit};
  text(20,detail::formatted(assets.target_formats[2],altitude));
  auto const separation{[&](unsigned int const axis) { return magnitude(difference(state.player[axis],target.position_q8[axis] >> 8)); }};
  int constexpr horizontal_units_per_range_step{660};
  int const range{word(magnitude(std::max(separation(0),separation(2)) / horizontal_units_per_range_step) + 1)};
  text(26,detail::formatted(assets.target_formats[3],range));
}
void target_display::marker(render::software_renderer &renderer, target_sight const sight, bool const acquired, vec2<int> const &centre, render::framebuffer *const coverage) const {
  bool const compact{sight == target_sight::compact};
  auto const shape{compact ? acquired ? instrument_mask::compact_air_lock : instrument_mask::compact_air_target : acquired ? instrument_mask::air_lock : instrument_mask::air_target};
  int const half_size{compact ? acquired ? 5 : 4 : acquired ? 10 : 8};
  render::draw_monochrome(renderer.output,assets.masks[std::to_underlying(shape)],{static_cast<uint16_t>(centre.x - half_size),static_cast<uint16_t>(centre.y - half_size)},6,coverage,renderer.clipping);
}
void target_display::edge_marker(render::software_renderer &renderer, target_display_state const &state, render::framebuffer *const coverage) const {
  // Native 0x7ee9c clips the line to the secondary marker's top-left corner,
  // then draws that marker at the clipped endpoint without painting the line.
  bool const compact{state.sight == target_sight::compact};
  int const half_size{compact ? 5 : 10};
  auto from{shifted({160,56},state.offset)};
  vec2<int> to{word((state.tracking_q8.x >> 8) - half_size),word((state.tracking_q8.y >> 8) - half_size)};
  auto const low{shifted(compact ? vec2<int>{132,23} : vec2<int>{116,18},state.offset)};
  auto const high{shifted(compact ? vec2<int>{177,78} : vec2<int>{193,94},state.offset)};
  presentation::clip_line(from,to,low,high);
  auto const shape{compact ? instrument_mask::compact_air_lock : instrument_mask::air_lock};
  render::draw_monochrome(renderer.output,assets.masks[std::to_underlying(shape)],{static_cast<uint16_t>(to.x),static_cast<uint16_t>(to.y)},6,coverage,renderer.clipping);
}
void target_display::direction(render::software_renderer &renderer, air_target_reading const &target, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *const coverage) const {
  // Native 0x7f000 draws this six-line cue only outside the central sight box.
  auto const low{shifted({116,18},state.offset)}, high{shifted({193,94},state.offset)};
  if(state.projected.x >= low.x && state.projected.x <= high.x && state.projected.y >= low.y && state.projected.y <= high.y) return;
  vec3<int32_t> const delta{difference(target.position_q8.x >> 8,state.player.x),difference(target.position_q8.y >> 8,state.player.y),difference(target.position_q8.z >> 8,state.player.z)};
  direction_marker(renderer,delta,state,minimum,maximum,coverage);
}
void target_display::ground(render::software_renderer &renderer, vec3<int32_t> const &target, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *const coverage) const {
  // Native 0x7f2a4 draws the projected cross when in range and always repeats
  // the six-line direction cue, even when the target lies inside the sight.
  auto const line{[&](vec2<int> const &a, vec2<int> const &b) {
    presentation::draw_clipped_line(renderer.output,vec2<int>{a},vec2<int>{b},minimum,maximum,6);
    if(coverage) presentation::draw_clipped_line(*coverage,vec2<int>{a},vec2<int>{b},minimum,maximum,255);
  }};
  if(std::max(magnitude(difference(state.player.x,target.x)),magnitude(difference(state.player.z,target.z))) < 32767 && state.projected.x >= 0 && state.projected.x < 320 && state.projected.y >= 0 && state.projected.y < 200) {
    auto const centre{vec2<int>{state.projected}};
    line(centre + vec2<int>{-5,-5},centre + vec2<int>{5,5});
    line(centre + vec2<int>{5,-5},centre + vec2<int>{-5,5});
  }
  direction_marker(renderer,{difference(target.x,state.player.x),difference(target.y,state.player.y),difference(target.z,state.player.z)},state,minimum,maximum,coverage);
}
void target_display::direction_marker(render::software_renderer &renderer, vec3<int32_t> const &delta, target_display_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *const coverage) const {
  auto const dot{[&](vec3<int32_t> const &axis) {
    uint32_t result{0};
    for(unsigned int index{0}; index != 3; ++index) result += static_cast<uint32_t>(delta[index]) * static_cast<uint32_t>(axis[index]);
    return std::bit_cast<int32_t>(result);
  }};
  // Attitude stores the aircraft axes as world-space columns. Native
  // DATA 0x1345e stores the same matrix by rows; no extra transpose belongs here.
  int32_t const horizontal{dot(state.orientation.right)};
  int32_t x{magnitude(horizontal)}, y{dot(state.orientation.down)};
  while(x < -32768 || x > 32767 || y < -32768 || y > 32767) { x >>= 1;y >>= 1; }
  // Native 0x7ef94 returns phase zero if either input lies between -1 and 1.
  math::angle angle{0};
  if((x < -1 || x > 1) && (y < -1 || y > 1)) angle = math::direction_angle(static_cast<int16_t>(x),static_cast<int16_t>(y));
  if(horizontal < 0) angle = static_cast<math::angle>(-angle);

  int constexpr radius{35}, half_size{5}, sine_unit{32767};
  vec2<int> const centre{word(160 + state.offset.x + math::sine_q15(angle) * radius / sine_unit),word(56 + state.offset.y + math::cosine_q15(angle) * radius / sine_unit)};
  std::array<vec2<int>,4> const corners{{{-half_size,0},{0,-half_size},{half_size,0},{0,half_size}}};
  auto const line{[&](vec2<int> const &a, vec2<int> const &b) {
    presentation::draw_clipped_line(renderer.output,centre + a,centre + b,minimum,maximum,6);
    if(coverage) presentation::draw_clipped_line(*coverage,centre + a,centre + b,minimum,maximum,255);
  }};
  line({-half_size,-half_size},{half_size,half_size});line({-half_size,half_size},{half_size,-half_size});
  for(unsigned int index{0}; index != corners.size(); ++index) line(corners[index],corners[(index + 1) % corners.size()]);
}

} // namespace tfx::cockpit
