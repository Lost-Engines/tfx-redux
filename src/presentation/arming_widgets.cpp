#include "arming_widgets.h"
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tfx::presentation {
namespace {

std::string number(std::string const &format, double const value) {
  // Formats come only from the checksum-verified 2.05 executable, never from player input.
  std::array<char,128> buffer{};
  int const size{std::snprintf(buffer.data(),buffer.size(),format.c_str(),value)};
  if(size < 0 || static_cast<unsigned int>(size) >= buffer.size()) throw std::runtime_error{"arming statistic exceeds its display buffer"};
  return {buffer.data(),static_cast<unsigned int>(size)};
}
void label(render::software_renderer &renderer, std::string const &text, vec2<int> const &shadow, vec2<int> const &foreground) {
  renderer.text(shadow.x,shadow.y,text,0);
  renderer.text(foreground.x,foreground.y,text,1);
}
void outline(render::software_renderer &renderer, std::array<vec2<int16_t>,4> const &corners) {
  for(unsigned int corner{0}; corner != corners.size(); ++corner) renderer.line(vec2d{corners[corner] + vec2<int16_t>{1,1}},vec2d{corners[(corner + 1) % corners.size()] + vec2<int16_t>{1,1}},0);
  for(unsigned int corner{0}; corner != corners.size(); ++corner) renderer.line(vec2d{corners[corner]},vec2d{corners[(corner + 1) % corners.size()]},1);
}

} // anonymous namespace

namespace {
void panel(render::software_renderer &renderer, std::array<vec2<int16_t>,4> const &corners, uint8_t const colour, vec2<int16_t> const &offset) {
  // The native UI rectangle includes both far edges, unlike the inspection polygon painter.
  int left{render::screen_width}, right{-1}, top{render::screen_height}, bottom{-1};
  for(auto const &corner : corners) {
    left = std::min(left,corner.x + offset.x);
    right = std::max(right,corner.x + offset.x);
    top = std::min(top,corner.y + offset.y);
    bottom = std::max(bottom,corner.y + offset.y);
  }
  left = std::max(left,0);
  right = std::min(right,render::screen_width - 1);
  top = std::max(top,0);
  bottom = std::min(bottom,render::screen_height - 1);
  if(left > right || top > bottom) return;
  for(int y{top}; y <= bottom; ++y) for(int x{left}; x <= right; ++x) renderer.output.write(static_cast<size_t>(y * render::screen_width + x),colour);
}
} // anonymous namespace

void draw_arming_unavailable(render::software_renderer &renderer, arming_assets const &assets) {
  panel(renderer,assets.unavailable_box,0,{0,0});
  panel(renderer,assets.unavailable_box,76,{-1,-1});
  renderer.text(127,96,assets.unavailable[0],15);
  renderer.text(127,106,assets.unavailable[1],15);
  renderer.text(126,95,assets.unavailable[0],56);
  renderer.text(126,105,assets.unavailable[1],56);
}
namespace {
void caption_panel(render::software_renderer &renderer, std::string const &label, vec2<int> const &position, int const half_width) {
  auto const x{static_cast<int16_t>(position.x)}, y{static_cast<int16_t>(position.y)};
  auto const left{static_cast<int16_t>(x - half_width - 4)}, right{static_cast<int16_t>(x + half_width + 2)};
  auto const top{static_cast<int16_t>(y - 2)}, bottom{static_cast<int16_t>(y + 6)};
  std::array<vec2<int16_t>,4> const corners{{{left,top},{right,top},{right,bottom},{left,bottom}}};
  panel(renderer,corners,0,{0,0});
  panel(renderer,corners,76,{1,1});
  renderer.text(x + 2 - half_width,y + 2,label,15);
  renderer.text(x + 1 - half_width,y + 1,label,56);
}

} // anonymous namespace

void draw_arming_store_label(render::software_renderer &renderer, std::string const &label, vec2<int> const &position) {
  caption_panel(renderer,label,position,static_cast<int>(label.size()) * 3);
}
void draw_arming_mission_incomplete(render::software_renderer &renderer, arming_assets const &assets) {
  // Native 8A66C calls 89268 with twice the message length, unlike store labels.
  caption_panel(renderer,assets.mission_incomplete,{160,100},static_cast<int>(assets.mission_incomplete.size()) * 2);
}

flight::store_effects draw_arming_widgets(render::software_renderer &renderer, arming_assets const &assets,
  arming_state const &state, weapons::armament const &inventory, std::span<weapons::definition const> const weapons,
  flight::airframe const &profile, double const previous_drag) {
  auto const aircraft{std::to_underlying(profile.stores.aircraft) - 1};
  for(unsigned int row{0}; row != assets.rows.size(); ++row) {
    int const y{assets.rows[row].maximum.y - 7};
    label(renderer,weapons[state.first_store + row].label,{225,y + 1},{224,y});
  }
  if(aircraft == 0) label(renderer,number(assets.drag_format,previous_drag * 10000),{6,30},{7,31});
  label(renderer,assets.aircraft_names.at(aircraft),{6,13},{7,12});
  auto corners{assets.scrollbar};
  auto const top{static_cast<int16_t>(state.first_store * 121 / 17 + 24)};
  corners[0].y = top;
  corners[1].y = top;
  corners[2].y = static_cast<int16_t>(top + 10);
  corners[3].y = static_cast<int16_t>(top + 10);
  outline(renderer,corners);
  label(renderer,assets.maximum,{138,175},{137,174});
  uint16_t mass{0};
  flight::loadout stores;
  for(unsigned int slot{0}; slot != flight::store_slots; ++slot) {
    auto const &pylon{inventory.pylons[slot]};
    if(pylon.weapon == weapons::empty_pylon) continue;
    auto const &weapon{weapons[pylon.weapon]};
    mass = static_cast<uint16_t>(mass + weapon.mass);
    stores[slot] = {
      .position{vec3d{profile.mounts[slot]}},
      .properties{
        .mass_kg{weapon.mass},
        .drag{weapon.drag},
        .fuel_tank{weapon.role == weapons::weapon_role::external_tank},
      },
      .occupied{pylon.count != 0},
    };
  }
  label(renderer,number(assets.mass_format,static_cast<double>(math::signed_word(mass) * static_cast<long double>(2.2))),{30,175},{29,174});
  if(!state.selected) label(renderer,assets.internal,{150,13},{149,12});
  else {
    auto const id{std::to_underlying(*state.selected)};
    auto const &weapon{weapons[id]};
    if(std::to_underlying(weapon.role) < assets.roles.size()) label(renderer,assets.roles[std::to_underlying(weapon.role)],{157,13},{156,12});
    if(weapon.category < assets.categories.size()) label(renderer,assets.categories[weapon.category],{170,13},{169,12});
    label(renderer,weapon.label,{225,175},{224,174});
    label(renderer,number(assets.store_mass_format,static_cast<double>(weapon.mass * static_cast<long double>(2.2))),{275,175},{274,174});
    if(aircraft == 0) label(renderer,number(assets.store_drag_format,static_cast<double>(static_cast<long double>(weapon.drag) * 10000)),{225,192},{224,191});
    if(id >= state.first_store && id < state.first_store + assets.rows.size()) {
      auto const &row{assets.rows[id - state.first_store]};
      vec2<int16_t> const low{row.minimum - vec2<int16_t>{1,1}}, high{row.maximum - vec2<int16_t>{1,1}};
      outline(renderer,{{vec2<int16_t>{low},{high.x,low.y},vec2<int16_t>{high},{low.x,high.y}}});
    }
  }
  auto const effects{flight::accumulate_stores(profile.stores,stores)};
  auto const percent{static_cast<double>(static_cast<long double>(effects.moment_a) / effects.total_moment_a * 100)};
  label(renderer,number(assets.inertia_format,percent),{6,22},{7,23});
  return effects;
}

} // namespace tfx::presentation
