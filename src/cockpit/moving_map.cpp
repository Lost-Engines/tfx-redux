#include "moving_map.h"
#include <algorithm>
#include <stdexcept>
#include "presentation/drawing.h"
#include "render/sprites.h"
#include "world/environment.h"

namespace tfx::cockpit {

moving_map::moving_map(resources::archive const &game, std::string const &name) {
  auto frames{resources::decode_sprites(game.read("spr/" + name + ".spr"))};
  if(frames.size() != 1 || frames.front().size != vec2<unsigned int>{world::map_width,world::map_height}) throw std::runtime_error{"unexpected original moving-map dimensions"};
  schematic = std::move(frames.front());
}
map_animation const &moving_map::state() const { return animation; }

void moving_map::draw(render::software_renderer &renderer, map_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *const coverage) {
  // Native 0x823d0 adds elapsed ticks per display draw and toggles at most once per call.
  uint32_t constexpr blink_ticks{50};
  animation.ticks += state.elapsed;
  if(std::bit_cast<int32_t>(animation.ticks) > static_cast<int32_t>(blink_ticks)) {
    animation.ticks -= blink_ticks;
    if(animation.target_colour == 11) { animation.target_colour = 15;animation.landing_colour = 6; }
    else { animation.target_colour = 11;animation.landing_colour = 7; }
  }
  vec2<int> const centre{minimum + (maximum - minimum) / 2};
  vec2<int> const low{minimum + vec2<int>{state.offset}}, high{maximum + vec2<int>{state.offset}};
  if(low.y > render::screen_height - 1) return;
  vec2<int> const clipped_low{low.x,std::max(0,low.y)}, clipped_high{high.x,std::min(render::screen_height - 1,high.y)};
  renderer.clipping = render::raster_clip::viewport(clipped_low,clipped_high);
  for(int y{clipped_low.y}; y <= clipped_high.y; ++y) for(int x{std::max(0,low.x)}; x <= std::min(render::screen_width - 1,high.x); ++x) {
    auto const index{static_cast<unsigned int>(y * render::screen_width + x)};
    renderer.output.write(index,128);
    if(coverage) coverage->pixels[index] = 255;
  }
  vec2<int> const player_cell{state.player.x / world::cell_units,state.player.z / world::cell_units};
  vec2<int> const map_centre{
    static_cast<uint16_t>(centre.x - player_cell.x + state.offset.x + world::map_width / 2),
    static_cast<uint16_t>(centre.y - (static_cast<int>(world::map_height) - player_cell.y) + state.offset.y + world::map_height / 2),
  };
  render::draw_sprite(renderer.output,schematic,map_centre,coverage,renderer.clipping);
  renderer.clipping = render::raster_clip::viewport({0,0},{render::screen_width - 1,render::screen_height - 1});
  vec2<int> const origin{centre.x + state.offset.x,centre.y + state.offset.y - 1};
  auto const direction{math::rotate_words({0,-8},state.heading)};
  vec2<int> const tip{
    math::signed_word(static_cast<uint16_t>(origin.x + direction.x)),
    math::signed_word(static_cast<uint16_t>(origin.y + direction.y)),
  };
  presentation::draw_clipped_line(renderer.output,vec2<int>{origin},vec2<int>{tip},{0,0},{render::screen_width - 1,render::screen_height - 1},15);
  if(coverage) presentation::draw_clipped_line(*coverage,vec2<int>{origin},vec2<int>{tip},{0,0},{render::screen_width - 1,render::screen_height - 1},255);
  auto const marker{[&](vec3<int32_t> const &position, uint8_t const colour) {
    vec2<int> const pixel{position.x / world::cell_units - player_cell.x + origin.x,player_cell.y - position.z / world::cell_units + origin.y};
    // Native 0x82164 excludes the border and the final screen row.
    if(pixel.x <= low.x || pixel.x >= high.x || pixel.y <= low.y || pixel.y >= high.y || pixel.y < 0 || pixel.y >= render::screen_height - 1) return;
    if(pixel.x < 0 || pixel.x >= render::screen_width) return;
    auto const index{static_cast<unsigned int>(pixel.y * render::screen_width + pixel.x)};
    renderer.output.write(index,colour);
    if(coverage) coverage->pixels[index] = 255;
  }};
  auto const aircraft{[&](std::span<vec3<int32_t> const> const contacts, uint8_t const colour) {
    for(auto const &position : contacts) if((position.y >> 8) < -37) marker({position.x >> 8,position.y >> 8,position.z >> 8},colour);
  }};
  auto const surface{[&](std::span<vec3<int32_t> const> const contacts, uint8_t const colour) {
    for(auto const &position : contacts) marker({position.x >> 8,position.y >> 8,position.z >> 8},colour);
  }};
  if(!state.stealth) {
    aircraft(state.contacts.enemy_aircraft,11);
    aircraft(state.contacts.allied_aircraft,6);
    surface(state.contacts.enemy_surface,12);
    surface(state.contacts.allied_surface,7);
  }
  for(auto const &target : state.targets) marker(target,animation.target_colour);
  if(state.landing_cell) marker({state.landing_cell->x * world::cell_units + world::cell_units / 2,0,state.landing_cell->y * world::cell_units + world::cell_units / 2},animation.landing_colour);
}

} // namespace tfx::cockpit
