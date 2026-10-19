#include "tactical_map.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>
#include "cockpit/format.h"
#include "presentation/drawing.h"
#include "render/sprites.h"
#include "world/environment.h"

namespace tfx::cockpit {
namespace {
int constexpr map_last_row{199}, line_last_row{191}, minimum_aircraft_y{-38};
unsigned int constexpr maximum_contact_cards{10};
uint32_t constexpr blink_ticks{35};
vec2<int> const line_minimum{0,0}, line_maximum{319,line_last_row};

int32_t difference(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? difference(0,value) : value;
}
} // anonymous namespace

tactical_map::tactical_map(resources::archive const &game, resources::executable_tables const &tables,
    std::string const &schematic_name, tactical_map_animation &animation)
  : assets{tables.instruments()}, renderer{tables}, overlay{game.read(assets.tactical_map_overlay)}, animation{animation} {
  auto frames{resources::decode_sprites(game.read("spr/" + schematic_name + ".spr"))};
  if(frames.size() != 1 || frames.front().size != vec2<unsigned int>{world::map_width,world::map_height}) throw std::runtime_error{"unexpected tactical-map dimensions"};
  schematic = std::move(frames.front());
}

auto tactical_map::point(vec3<int32_t> const &position, uint8_t const colour)->void {
  // Native 0x86c70 divides signed world coordinates towards zero, then passes
  // unsigned screen words to the point painter. Targets use shifts separately.
  unsigned int const x{static_cast<uint16_t>(position.x / world::cell_units)}, y{static_cast<uint16_t>(map_last_row - position.z / world::cell_units)};
  if(x < render::screen_width && y < render::screen_height) renderer.output.write(static_cast<unsigned int>(y * render::screen_width + x),colour);
}
auto tactical_map::target_points()->void {
  for(auto const &target : snapshot.targets) point({(target.x >> world::cell_shift) * world::cell_units,0,(target.z >> world::cell_shift) * world::cell_units},animation.target_colour);
}
auto tactical_map::landing_point(uint8_t const colour)->void {
  if(!snapshot.landing_cell) return;
  auto const &cell{*snapshot.landing_cell};
  point({cell.x * world::cell_units + world::cell_units / 2,0,cell.y * world::cell_units + world::cell_units / 2},colour);
}
auto tactical_map::contact_card(tactical_map_aircraft const &aircraft, unsigned int const index)->void {
  auto const &reading{aircraft.reading};
  std::string name{reading.name};
  for(char &character : name) if(character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
  uint8_t const colour{aircraft.selected ? uint8_t{11} : uint8_t{6}};
  int const x{index % 2 ? 270 : 221}, y{17 + static_cast<int>(index / 2) * 34};
  auto const text{[&](int const row, std::string const &message) { renderer.text(x,y + row * 6,message,colour); }};
  text(0,name);
  int16_t const speed{math::signed_word(static_cast<uint16_t>(reading.speed_q16 >> 16))};
  text(1,detail::decimal(assets.tactical_map_formats[0],static_cast<double>(static_cast<long double>(speed) * assets.speed_scale),0));
  unsigned int constexpr angle_units_per_degree{182}, feet_per_world_unit{8}, units_per_range_step{660};
  text(2,detail::formatted(assets.tactical_map_formats[1],static_cast<unsigned int>(reading.heading) / angle_units_per_degree));
  uint32_t const altitude{(0u - static_cast<uint32_t>(reading.position_q8.y >> 8)) * feet_per_world_unit};
  text(3,detail::formatted(assets.tactical_map_formats[2],static_cast<unsigned long>(altitude)));
  int32_t const dx{magnitude(difference(snapshot.player.x,reading.position_q8.x >> 8))}, dz{magnitude(difference(snapshot.player.z,reading.position_q8.z >> 8))};
  auto const range{magnitude(std::max(dx,dz) / static_cast<int32_t>(units_per_range_step))};
  text(4,detail::formatted(assets.tactical_map_formats[3],static_cast<long>(range)));
}

auto tactical_map::open(tactical_map_snapshot const &state)->void {
  snapshot = state;
  animation.ticks = 0;
  renderer.output.pixels.fill(0);
  renderer.clipping = render::raster_clip::viewport({0,0},{319,199});
  render::draw_sprite(renderer.output,schematic,{100,100},nullptr,renderer.clipping);
  overlay.image().paint(renderer.output);
  auto const line{[&](vec2<int> const &from, vec2<int> const &to, uint8_t const colour) {
    presentation::draw_clipped_line(renderer.output,vec2<int>{from},vec2<int>{to},line_minimum,line_maximum,colour);
  }};
  for(unsigned int index{1}; index < snapshot.route.size(); ++index) {
    auto const &a{snapshot.route[index - 1]}, &b{snapshot.route[index]};
    // Native 0x86d9c calls the raster directly; only the heading line uses
    // the shorter model viewport. Route points already belong to the map.
    presentation::draw_line(renderer.output,{a.x,map_last_row - a.y},{b.x,map_last_row - b.y},13);
  }
  unsigned int cards{0};
  for(auto const &aircraft : snapshot.hostile_aircraft) if((aircraft.reading.position_q8.y >> 8) < minimum_aircraft_y) {
    point({aircraft.reading.position_q8.x >> 8,aircraft.reading.position_q8.y >> 8,aircraft.reading.position_q8.z >> 8},11);
    if(cards < maximum_contact_cards) contact_card(aircraft,cards++);
  }
  for(auto const &aircraft : snapshot.allied_aircraft) if((aircraft.reading.position_q8.y >> 8) < minimum_aircraft_y) point({aircraft.reading.position_q8.x >> 8,aircraft.reading.position_q8.y >> 8,aircraft.reading.position_q8.z >> 8},6);
  for(auto const &position : snapshot.allied_surface) point({position.x >> 8,position.y >> 8,position.z >> 8},7);
  for(auto const &position : snapshot.hostile_surface) point({position.x >> 8,position.y >> 8,position.z >> 8},12);
  target_points();
  if(snapshot.landing_cell) {
    // Native 0x86fc6 mixes its first marker's register arguments. Preserve
    // that initial black pixel; later blink updates use the actual runway.
    point({snapshot.landing_cell->y * world::cell_units + world::cell_units / 2,0,animation.target_colour},0);
  }
  vec2<int> const player{snapshot.player.x / world::cell_units,map_last_row - snapshot.player.z / world::cell_units};
  auto const direction{math::rotate_words({0,-8},snapshot.heading)};
  line(player,player + vec2<int>{direction},15);
  point(snapshot.player,14);
}
auto tactical_map::advance(simulation::timer_ticks const elapsed)->void {
  animation.ticks += elapsed;
  if(math::signed_word(static_cast<uint16_t>(animation.ticks)) <= static_cast<int32_t>(blink_ticks)) return;
  animation.ticks -= blink_ticks;
  animation.target_colour = animation.target_colour == 11 ? 15 : 11;
  target_points();
  landing_point(animation.target_colour == 15 ? 6 : 7);
}
auto tactical_map::image() const->render::framebuffer const& { return renderer.output; }

} // namespace tfx::cockpit
