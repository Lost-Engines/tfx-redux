#include "music_control.h"
#include <cmath>

namespace tfx::audio {
namespace {

bool sound_blaster(uint8_t const device) { return device == 1 || device == 3 || device == 4; }
bool transient_allowed(music_control_state const &state, music_control_context const &context) {
  return !state.resume_cue && !context.inhibited && !context.collision && !context.aircraft_disabled;
}
std::optional<music_selection> select_lapc(music_control_state &state, music_control_context const &context, uint8_t const cue) {
  unsigned int slot{7};
  switch(cue) {
  case 0: slot = 6;break;
  case 1: slot = 2;break;
  case 3: slot = 1;break;
  case 5: slot = 0;break;
  case 6: slot = 9;break;
  case 7: slot = 4;break;
  case 8: slot = 5;break;
  case 9: case 10: case 11:
    if(!transient_allowed(state,context)) return {};
    state.resume_cue = state.last_cue ? state.last_cue : 2;
    slot = cue == 9 ? 3u : cue == 10 ? 10u : 11u;
    break;
  default: break;
  }
  if(cue < 9 || cue > 11) state.resume_cue = 0;
  return music_selection{
    .order{context.lapc_orders[slot]},
    .stop_previous{true}
  };
}
std::optional<music_selection> select_fm(music_control_state &state, music_control_context const &context, uint8_t cue) {
  if(sound_blaster(context.sound_device) && context.throttle != 0 &&
    (((context.mode == music_mode::soundblaster_cd || context.mode == music_mode::fm) && !context.music_playing) || context.mode == music_mode::none)) cue = 12;
  if(cue != 12 && !context.enabled) return {};
  music_selection selection;
  switch(cue) {
  case 0: return selection; // Start the resident resource at order zero.
  case 1: case 5: selection.resource_index = 19;break;
  case 3: selection.resource_index = 20;break;
  case 6: selection.resource_index = 18;selection.order = 8;break;
  case 7: selection.resource_index = 18;selection.order = 5;break;
  case 8: selection.resource_index = 18;selection.order = 7;break;
  case 9: case 10: case 11: {
    if(!transient_allowed(state,context)) return {};
    auto const resume{state.last_cue ? state.last_cue : 2};
    selection.resource_index = 18;
    selection.order = cue == 9 ? 3 : cue == 10 ? 1 : 0;
    state.resume_cue = static_cast<int16_t>(resume);
    break;
  }
  case 12: {
    if(context.mode == music_mode::lapc || context.mode == music_mode::sound_canvas || !sound_blaster(context.sound_device)) return {};

    double constexpr idle_throttle{.6}, orders_per_throttle{10};
    // The original truncates after an x87 double-precision calculation, then
    // retains only the low byte when selecting the engine bank's order.
    selection.order = static_cast<uint8_t>(static_cast<int16_t>(std::trunc((static_cast<double>(context.throttle) - idle_throttle) * orders_per_throttle)));
    selection.resource_index = 21;
    state.engine_playing = true;
    break;
  }
  case 13: return {};
  default: selection.resource_index = 17;break;
  }
  if(cue < 9 || cue > 11) state.resume_cue = 0;
  selection.replace_resource = true;
  return selection;
}

} // namespace

std::optional<music_selection> select_flight_music(music_control_state &state,
    music_control_context const &context, uint8_t const cue) {
  if(context.cd_playing || context.sound_device == 2 || context.preselected) return {};
  if(context.mode != music_mode::soundblaster_cd && context.mode != music_mode::lapc && context.mode != music_mode::fm && context.mode != music_mode::sound_canvas) return {};
  auto result{context.mode == music_mode::lapc ? select_lapc(state,context,cue) : select_fm(state,context,cue)};
  if(result) state.last_cue = cue; // Retain the requested cue, even when FM substitutes engine audio.
  return result;
}

music_control_update update_flight_music(music_control_state &state,
    music_control_context const &context, bool const playing) {
  music_control_update result;
  if(context.cd_playing && context.mode == music_mode::soundblaster_cd) return result;
  bool const engine_mode{((context.mode == music_mode::soundblaster_cd || context.mode == music_mode::fm) && !context.enabled) ||
    (context.mode == music_mode::none && sound_blaster(context.sound_device))};
  if(engine_mode) {

    auto const difference{static_cast<double>(context.throttle) - state.previous_throttle};
    double constexpr throttle_step{.1};
    float constexpr running_throttle{.59f};
    if((difference > throttle_step || difference < -throttle_step) && context.throttle > running_throttle) {
      state.engine_playing = false;
      state.previous_throttle = context.throttle;
    }
    if((!context.engines_running && state.engine_playing) || context.inhibited) {
      state.engine_playing = false;
      result.stop_and_release = context.mode != music_mode::none;
    } else if(!state.engine_playing && !context.music_playing && sound_blaster(context.sound_device) && context.engines_running) result.selection = select_flight_music(state,context,12);
  }
  bool const supported{context.mode == music_mode::soundblaster_cd || context.mode == music_mode::lapc || context.mode == music_mode::fm || context.mode == music_mode::sound_canvas};
  if(supported && state.resume_cue && !result.selection && (!playing || result.stop_and_release)) {
    auto const cue{static_cast<uint8_t>(state.resume_cue)};
    state.resume_cue = 0;
    result.selection = select_flight_music(state,context,cue);
  }
  return result;
}

} // namespace tfx::audio
