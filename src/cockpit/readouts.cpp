#include "readouts.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>
#include "render/sprites.h"

namespace tfx::cockpit {
namespace {

uint8_t constexpr panel_colour{107};

template<typename Value>
std::string formatted(std::string const &format, Value const value) {
  // Formats come only from the verified reference executable, never from user input.
  std::array<char,128> buffer{};
  int const length{std::snprintf(buffer.data(),buffer.size(),format.c_str(),value)};
  if(length < 0 || static_cast<unsigned int>(length) >= buffer.size()) throw std::runtime_error{"instrument number exceeds its display buffer"};
  return {buffer.data(),static_cast<unsigned int>(length)};
}

vec2<int> shifted(vec2<int> const &position, vec2<int16_t> const &offset) {
  // Original coordinates add in 16-bit registers before sign extension.
  return {math::signed_word(static_cast<uint16_t>(position.x + offset.x)),math::signed_word(static_cast<uint16_t>(position.y + offset.y))};
}

bool clear_panel(render::software_renderer &renderer, vec2<int> const &minimum, vec2<int> const &maximum,
  vec2<int16_t> const &offset, bool const exclude_last_row, render::framebuffer *const coverage) {
  auto const low{shifted(minimum,offset)}, high{shifted(maximum,offset)};
  int constexpr last_row{render::screen_height - 1};
  if(low.y > last_row || (exclude_last_row && low.y == last_row)) return false;
  // Native 0xc98d8 includes both far edges. Each instrument clips its bottom first.
  for(int y{std::max(0,low.y)}; y <= std::min(last_row,high.y); ++y) {
    for(int x{std::max(0,low.x)}; x <= std::min(render::screen_width - 1,high.x); ++x) {
      auto const index{static_cast<unsigned int>(y * render::screen_width + x)};
      renderer.output.write(index,panel_colour);
      if(coverage) coverage->pixels[index] = 255;
    }
  }
  return true;
}

} // anonymous namespace

readouts::readouts(instrument_assets const &source, resources::archive const &game) : assets{source}, digits{resources::decode_sprites(game.read(assets.font_bank))}, autopilot_lamp{resources::decode_sprites(game.read(assets.autopilot_bank))} {
}
void readouts::number(render::software_renderer &renderer, vec2<int> const &position, std::string_view const text, render::framebuffer *const coverage) const {
  // Native 0x826b4 uses a coloured sprite font with a fixed four-pixel advance.
  int x{position.x};
  for(unsigned char const character : text) {
    if(!character) break;
    if(character == ' ') { x += 4; continue; }
    if(character == '\r' || character == '\n') continue;
    auto const index{assets.character_map[character & 63]};
    if(index == 255) continue;
    vec2<int> const centre{static_cast<uint16_t>(x + 4),static_cast<uint16_t>(position.y + 2)};
    render::draw_sprite(renderer.output,digits.at(index),centre,coverage,renderer.clipping);
    x += 4;
  }
}
void readouts::draw_radio(render::software_renderer &renderer, std::string_view const message, bool const forward, vec2<int16_t> const &offset, render::framebuffer *const coverage) const {
  if(forward) number(renderer,shifted({122,122},offset),message,coverage);
  else renderer.text(122,179,message,6,coverage);
}
void readouts::draw_autopilot(render::software_renderer &renderer, autopilot_display_state const &state, vec2<int16_t> const &offset, render::framebuffer *const coverage) const {
  // Native 0x7e1f4 always prints the selected mode number, including while disengaged.
  if(state.enabled) {
    auto const position{shifted({22,140},offset)};
    render::draw_sprite(renderer.output,autopilot_lamp.at(0),{static_cast<uint16_t>(position.x),static_cast<uint16_t>(position.y)},coverage,renderer.clipping);
  }
  auto const position{shifted({26,138},offset)};
  int const mode{math::signed_word(std::to_underlying(state.mode)) + 1};
  renderer.text(position.x,position.y,formatted(assets.autopilot_number_format,mode),1,coverage);
}
fuel_warning readouts::draw(render::software_renderer &renderer, readout_state const &state, render::framebuffer *const coverage) const {
  // Native 0x813c4: a wrapping sum, then hours/minutes/seconds at 54 ticks per second.
  if(clear_panel(renderer,{8,153},{32,157},state.offset,false,coverage)) {
    uint32_t const ticks{state.elapsed_ticks + state.clock_offset};
    unsigned long const hours{ticks / (instrument_ticks_per_second * 3600)};
    unsigned long const minutes{(ticks / (instrument_ticks_per_second * 60)) % 60};
    long const seconds{(ticks / instrument_ticks_per_second) % 60};
    number(renderer,shifted({7,153},state.offset),formatted(assets.unsigned_clock_formats[hours >= 10],hours),coverage);
    number(renderer,shifted({16,153},state.offset),formatted(assets.unsigned_clock_formats[minutes >= 10],minutes),coverage);
    number(renderer,shifted({25,153},state.offset),formatted(assets.signed_clock_formats[seconds >= 10],seconds),coverage);
  }
  auto const fuel_status{state.fuel < assets.critical_fuel ? fuel_warning::critical : state.fuel < assets.low_fuel ? fuel_warning::low : fuel_warning::none};
  // Native 0x81558 changes both colour and font as fuel crosses the warning thresholds.
  if(clear_panel(renderer,{14,172},{32,178},state.offset,false,coverage)) {
    auto const text{formatted(assets.fuel_format,state.fuel)};
    if(fuel_status == fuel_warning::none) number(renderer,shifted({16,172},state.offset),text,coverage);
    else {
      auto const position{shifted({17,172},state.offset)};
      renderer.text(position.x,position.y,text,fuel_status == fuel_warning::critical ? 11 : 14,coverage);
    }
  }
  if(clear_panel(renderer,{14,184},{33,190},state.offset,false,coverage)) {
    // Native 0xc03cc rounds the extended product towards zero before FISTP.
    int const percent{static_cast<int>(std::trunc(static_cast<long double>(state.throttle) * assets.throttle_percent))};
    unsigned int const format{percent >= 100 ? 2u : percent >= 10 ? 1u : 0u};
    number(renderer,shifted({17,184},state.offset),formatted(assets.throttle_formats[format],percent),coverage);
  }
  // Native 0x8176c: these three panels skip the last row itself, unlike the others.
  if(clear_panel(renderer,{292,156},{307,161},state.offset,true,coverage)) {
    if(state.ecm) number(renderer,shifted({294,156},state.offset),assets.ecm_label,coverage);
    else if(state.stealth && state.aircraft != flight::player_aircraft::nighthawk) number(renderer,shifted({292,156},state.offset),assets.stealth_label,coverage);
  }
  if(clear_panel(renderer,{299,168},{306,174},state.offset,true,coverage)) number(renderer,shifted({299,168},state.offset),formatted(assets.decoy_formats[state.chaff >= 10],static_cast<int>(state.chaff)),coverage);
  if(clear_panel(renderer,{297,181},{304,186},state.offset,true,coverage)) number(renderer,shifted({297,181},state.offset),formatted(assets.decoy_formats[state.flares >= 10],static_cast<int>(state.flares)),coverage);
  return fuel_status;
}

} // namespace tfx::cockpit
