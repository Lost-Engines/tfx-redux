#include "hud.h"
#include "vector_hud.h"
#include <stdexcept>
#include <utility>

namespace tfx::cockpit {

void hud_selection::cycle_left(bool const has_ground_targets) {
  left = static_cast<uint16_t>(left + 1);
  if(left == 7 && !has_ground_targets) ++left;
}
void hud_selection::cycle_centre(flight::player_aircraft const aircraft) {
  auto mode{static_cast<uint16_t>(std::to_underlying(centre) + 1)};
  if(aircraft == flight::player_aircraft::nighthawk && mode == 1) mode = 2;
  centre = static_cast<centre_display>(mode);
}
void hud_selection::cycle_right(flight::player_aircraft const aircraft, uint8_t const weapon_category) {
  right = static_cast<uint16_t>(right + 1);
  if(right == 6 && weapon_category != 3 && aircraft != flight::player_aircraft::nighthawk) ++right;
}
void hud_selection::cycle_range() {
  if(centre == centre_display::horizontal_radar) {
    horizontal_range = static_cast<uint16_t>(horizontal_range - 1);
    if(math::signed_word(horizontal_range) < 0) horizontal_range = 3;
  } else if(centre == centre_display::bvr_radar) {
    bvr_range = static_cast<uint16_t>(bvr_range - 1);
    if(math::signed_word(bvr_range) < 0) bvr_range = 2;
  }
}
void hud_selection::normalise() {
  // Original draw-time signed-word checks; the key callbacks only increment.
  if(math::signed_word(left) >= 9) left = 0;
  if(math::signed_word(right) >= 9) right = 0;
  if(math::signed_word(std::to_underlying(centre)) > 2) centre = centre_display::horizontal_radar;
}

hud::hud(instrument_assets const &source, resources::archive const &game, std::span<weapons::definition const> const definitions, std::string const &schematic)
  : assets{source}, weapons{definitions}, numbers{assets,game}, displays{assets,game,weapons}, map{game,schematic}, radar_display{assets,game}, warnings{assets,game}, auxiliary{assets,game} {
}
void hud::draw_radio(render::software_renderer &renderer, std::string_view const message) const {
  renderer.clipping = render::raster_clip::viewport({0,0},{render::screen_width - 1,render::screen_height - 1});
  numbers.draw_radio(renderer,message,false);
}
hud_animation hud::animation() const {
  return {
    .warnings{warnings.state()},
    .auxiliary{auxiliary.state()},
    .map{map.state()},
    .radar_pulse{radar_display.phase()},
    .redraw_phase{redraw_phase},
  };
}
hud_effects hud::draw(render::software_renderer &renderer, hud_selection &selection, hud_state const &state, math::random_state &random, render::framebuffer *const coverage, scene_display const &scene, vector_hud *const optical) {
  // Native 0x803e8: fuel status changes before either MFD or the warning lamps reads it.
  renderer.clipping = render::raster_clip::viewport({0,0},{render::screen_width - 1,render::screen_height - 1});
  redraw_phase = state.redraw_phase;
  auto const &offset{state.instruments.offset};
  if(state.in_range) {
    capture_hud capture{renderer.output,optical};
    renderer.text(math::signed_word(static_cast<uint16_t>(96 + offset.x)),math::signed_word(static_cast<uint16_t>(73 + offset.y)),assets.in_range_label,6,coverage);
  }
  auto const fuel{numbers.draw(renderer,{
    .elapsed_ticks{state.ticks},
    .clock_offset{state.clock_offset},
    .fuel{state.instruments.fuel},
    .throttle{state.instruments.throttle},
    .aircraft{state.instruments.aircraft},
    .ecm{state.ecm},
    .stealth{state.stealth},
    .chaff{state.chaff},
    .flares{state.flares},
    .offset{vec2<int16_t>{offset}},
  },coverage)};
  mfd_state instruments{state.instruments};
  instruments.systems[std::to_underlying(flight::aircraft_system::fuel)] = fuel == fuel_warning::none ? flight::system_condition::active : fuel == fuel_warning::low ? flight::system_condition::intermittent : flight::system_condition::failed;
  std::array<std::vector<vec3<int32_t>>,4> positions;
  auto const copy_positions{[&](unsigned int const group, auto const contacts) {
    for(auto const &contact : contacts) positions[group].emplace_back(contact.position_q8);
  }};
  copy_positions(0,state.contacts.enemy_aircraft);
  copy_positions(1,state.contacts.allied_aircraft);
  copy_positions(2,state.contacts.enemy_surface);
  copy_positions(3,state.contacts.allied_surface);
  map_state const map_inputs{
    .player{vec3<int32_t>{state.position}},
    .heading{instruments.heading},
    .elapsed{state.elapsed},
    .stealth{state.stealth},
    .contacts{
      .enemy_aircraft{positions[0]},
      .allied_aircraft{positions[1]},
      .enemy_surface{positions[2]},
      .allied_surface{positions[3]},
    },
    .targets{state.targets},
    .landing_cell{state.landing_cell},
    .offset{vec2<int16_t>{offset}},
  };
  selection.normalise();
  auto const outer{[&](display_side const side, unsigned int const selected) {
    unsigned int const normalized{selected < 9 ? selected : 0};
    auto &last_page{last_outer_pages[std::to_underlying(side)]};
    if(normalized != last_page) redraw_phase = 3; // Native dispatchers request a three-page refresh.
    last_page = static_cast<uint8_t>(normalized);
    auto const page{outer_page(side,normalized)};
    if(page == display_page::moving_map) map.draw(renderer,map_inputs,{213,133},{264,167},coverage);
    else if(page == display_page::camera || page == display_page::ground_database) {
      if(!scene) throw std::runtime_error{"scene MFD has no camera renderer"};
      scene(side,page,instruments);
    } else displays.draw(renderer,side,page,instruments,coverage);
  }};
  outer(display_side::left,selection.left);
  auto const centre{std::to_underlying(selection.centre) <= std::to_underlying(centre_display::moving_map) ? selection.centre : centre_display::horizontal_radar};
  if(centre == centre_display::moving_map) map.draw(renderer,map_inputs,{135,151},{186,193},coverage);
  else {
    weapons::weapon_role role{weapons::weapon_role::air};
    if(instruments.inventory && !instruments.inventory->guns) role = weapons[instruments.inventory->pylons.at(static_cast<unsigned int>(instruments.inventory->selected)).weapon].role;
    radar_display.draw(renderer,centre == centre_display::bvr_radar ? radar_mode::beyond_visual_range : radar_mode::horizontal,
      centre == centre_display::bvr_radar ? selection.bvr_range : selection.horizontal_range,{
      .player{vec3<int32_t>{state.position}},
      .heading{instruments.heading},
      .aircraft{instruments.aircraft},
      .condition{instruments.systems[std::to_underlying(flight::aircraft_system::radar)]},
      .stealth{state.stealth},
      .guns{!instruments.inventory || instruments.inventory->guns},
      .selected_role{role},
      .enemy_aircraft{state.contacts.enemy_aircraft},
      .allied_aircraft{state.contacts.allied_aircraft},
      .enemy_surface{state.contacts.enemy_surface},
      .allied_surface{state.contacts.allied_surface},
      .projectiles{state.contacts.projectiles},
      .offset{vec2<int16_t>{offset}},
    },random,coverage);
  }
  outer(display_side::right,selection.right);
  bool const threat_sound{warnings.draw(renderer.output,{
    .systems{instruments.systems},
    .autopilot{instruments.controls.autopilot.enabled},
    .hook{state.hook},
    .fire_permission_notice{state.fire_permission_notice},
    .tracked_weapon_category{state.tracked_weapon_category},
    .stall_recovery{state.stall_recovery},
    .threat{state.threat},
    .offset{vec2<int16_t>{offset}},
  },coverage,renderer.clipping)};
  numbers.draw_autopilot(renderer,instruments.controls.autopilot,offset,coverage);
  auxiliary.draw(renderer.output,{
    .engines{instruments.systems[std::to_underlying(flight::aircraft_system::left_engine)],instruments.systems[std::to_underlying(flight::aircraft_system::right_engine)]},
    .redraw_phase{redraw_phase},
    .afterburner_pips{state.afterburner_pips},
    .approach_cue{state.approach_cue},
    .approach_suppressed{state.approach_suppressed},
    .low_speed_ground_contact{state.low_speed_ground_contact},
    .landed{state.landed},
    .offset{vec2<int16_t>{offset}},
  },coverage,renderer.clipping);
  if(state.radio_message) numbers.draw_radio(renderer,*state.radio_message,true,offset,coverage);
  return {
    .fuel_condition{instruments.systems[std::to_underlying(flight::aircraft_system::fuel)]},
    .threat_sound{threat_sound},
  };
}

} // namespace tfx::cockpit
