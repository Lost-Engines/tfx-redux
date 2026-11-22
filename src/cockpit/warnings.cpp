#include "warnings.h"
#include <algorithm>
#include <utility>
#include "render/sprites.h"

namespace tfx::cockpit {

warning_lamps::warning_lamps(instrument_assets const &source, resources::archive const &game) : assets{source} {
  for(unsigned int bank{0}; bank != banks.size(); ++bank) banks[bank] = resources::decode_sprites(game.read(assets.warning_banks[bank]));
}
warning_animation const &warning_lamps::state() const {
  return animation;
}

bool warning_lamps::draw(render::framebuffer &target, warning_state const &state, render::framebuffer *const coverage, render::raster_clip const &clip) {
  // Native 0x81a48 advances its four-draw phase even when no warning lamp is visible.
  unsigned int constexpr phase_frames{4};
  unsigned int constexpr phase_count{32};
  if(animation.countdown) {
    --animation.countdown;
  } else {
    animation.countdown = phase_frames - 1;
    animation.phase = static_cast<uint16_t>((animation.phase + 1) & (phase_count - 1));
  }
  auto const sprite{[&](unsigned int const bank, unsigned int const frame, vec2<int> const &position){
    vec2<int> const centre{static_cast<uint16_t>(position.x + state.offset.x), static_cast<uint16_t>(position.y + state.offset.y)};
    render::draw_sprite(target, banks.at(bank).at(frame), centre, coverage, clip);
  }};
  auto const condition{[&](flight::aircraft_system const system){return state.systems[std::to_underlying(system)];}};
  auto const gear{condition(flight::aircraft_system::landing_gear)};
  if(gear != flight::system_condition::normal) sprite(0, assets.condition_frames.at(std::to_underlying(gear)), {89, 109});
  if(state.autopilot) sprite(0, 7, {108, 109});
  auto const brake{condition(flight::aircraft_system::wheel_brakes)};
  if(brake != flight::system_condition::normal) sprite(0, assets.condition_frames.at(std::to_underlying(brake)) + 8u, {89, 118});
  if(state.hook) sprite(0, 15, {108, 118});
  if(std::ranges::find(state.systems, flight::system_condition::failed) != state.systems.end()) sprite(1, 0, {135, 105});
  if(std::ranges::find(state.systems, flight::system_condition::intermittent) != state.systems.end()) sprite(1, 1, {161, 105});
  if(condition(flight::aircraft_system::left_engine) == flight::system_condition::failed || condition(flight::aircraft_system::right_engine) == flight::system_condition::failed) sprite(1, 2, {187, 105});
  if(state.fire_permission_notice) sprite(1, 5, {217, 119});
  if(state.tracked_weapon_category && *state.tracked_weapon_category != 7) {
    sprite(1, 6, {231, 119});
    if(*state.tracked_weapon_category == 0) {
      sprite(1, 3, {217, 109});
    } else if(*state.tracked_weapon_category == 1) sprite(1, 4, {231, 109});
  }
  if(state.stall_recovery) sprite(1, 7, {69, 119});
  bool const play_sound{state.threat && !animation.threat_sound_played};
  if(state.threat) {
    animation.threat_sound_played = true;                                      // The original latch is not cleared when the warning disappears.
    sprite(1, 8, {251, 120});
  }
  return play_sound;
}

} // namespace tfx::cockpit
