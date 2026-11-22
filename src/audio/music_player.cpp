#include "music_player.h"
#include <algorithm>
#include "audio/la_synth.h"

namespace tfx::audio {

music_player::music_player(resources::archive const &game, std::span<uint8_t const> const launcher,
    std::filesystem::path const &directory, unsigned int const output_rate, bool const enabled) :
    music_player{game,launcher,make_la_music_device(launcher,std::make_unique<la_synth>(directory,output_rate)),output_rate,enabled} {}
music_player::music_player(resources::archive const &game, std::span<uint8_t const> const launcher,
    std::unique_ptr<music_device> device, unsigned int const output_rate, bool const enabled) :
    game{game}, catalogue{launcher}, device{std::move(device)}, clock{output_rate}, enabled{enabled}, ticks_enabled{enabled} {
  if(!this->device) throw std::invalid_argument{"music player requires a device"};
  sequencer.register_device(this->device->driver());
  this->device->driver().initialise();
  // Let the reset settle before the first musical timeline. Uploading patches
  // later does not reset the chip, its reverb or the persistent UART stream.
  std::vector<float> settling(this->device->reset_settling_frames(output_rate) * 2u);
  this->device->render(settling);
}
void music_player::stop_playing_locked() {
  if(playing) { sequencer.stop(*playing);playing.reset(); }
}
void music_player::stop_locked() {
  stop_playing_locked();
  if(resource && !effect_bank) sequencer.stop_all();
  if(resource) { sequencer.release(*resource);resource.reset(); }
  effect_bank = false;
}
void music_player::replace_locked(uint8_t const index) {
  stop_locked();
  auto const files{catalogue.files(device->driver().device_type())};
  if(index >= files.size()) return;                                            // Native missing filename/resource result.
  if(game.contains(files[index])) resource = sequencer.load(game.read(files[index]));
}
void music_player::configure(music_mode const mode, uint8_t const sound_device, lapc_order_profiles const &orders) {
  std::lock_guard lock{mutex};
  profiles = orders;
  context.mode = mode;context.sound_device = sound_device;
  context.enabled = context.music_playing = enabled;
  context.lapc_orders = profiles[profile];
}
void music_player::context_locked(flight_audio_state const &state) {
  context.throttle = state.throttle;context.engines_running = state.engines_running;
  context.inhibited = state.inhibited;context.collision = state.collision;context.aircraft_disabled = state.aircraft_disabled;
  context.preselected = preselected.has_value();context.enabled = enabled;
}
void music_player::apply_locked(music_selection const &selection) {
  if(selection.replace_resource) replace_locked(selection.resource_index);
  else if(selection.stop_previous) stop_playing_locked();
  if(resource) playing = sequencer.play(*resource,0,selection.order);
}
void music_player::cue_locked(uint8_t const value) {
  if(auto const selection{select_flight_music(control,context,value)}) apply_locked(*selection);
}
void music_player::set_cd_mode(bool const active) {
  std::lock_guard lock{mutex};
  context.cd_playing = active;
}
std::optional<uint8_t> music_player::preselected_index() const {
  std::lock_guard lock{mutex};
  return preselected;
}
void music_player::begin_flight(flight_audio_state const &state) {
  std::lock_guard lock{mutex};
  flight_active = true;
  context_locked(state);
  auto const selected{preselected.value_or(10)};
  preselected.reset();context.preselected = false;
  if(context.cd_playing) { stop_locked();return; }
  if(context.sound_device == 2) {
    // Native A5FC8 requests filename slot 17 and loads it independently of
    // music enable. Individual A6068 callers start orders in this bank.
    replace_locked(17);
    effect_bank = true;
    control.last_cue = 0;
    return;
  }
  if(!enabled) return;
  replace_locked(selected);control.last_cue = 0;
  cue_locked(0);
}
void music_player::end_flight() {
  std::lock_guard lock{mutex};
  stop_locked();
  flight_active = false;
}
void music_player::update_flight(flight_audio_state const &state) {
  std::lock_guard lock{mutex};
  if(!flight_active) return;
  context_locked(state);
  bool const active{playing && (sequencer.state(*playing).flags & 1)};
  auto const update{update_flight_music(control,context,active)};
  if(update.stop_and_release) stop_locked();
  if(update.selection) apply_locked(*update.selection);
}
void music_player::cue(uint8_t const value) {
  std::lock_guard lock{mutex};
  if(flight_active) cue_locked(value);
}
void music_player::effect(lapc_effect_cue const cue) {
  std::lock_guard lock{mutex};
  if(!flight_active || context.sound_device != 2 || !resource) return;
  stop_playing_locked();
  // The original ignores a full channel pool. Existing effects keep their
  // voices and expire through the sequencer, rather than being preempted.
  static_cast<void>(sequencer.try_play(*resource,0,std::to_underlying(cue)));
}
void music_player::select(int16_t const requested) {
  std::lock_guard lock{mutex};
  flight_active = false;
  uint8_t index{static_cast<uint8_t>(requested)};
  if(!preselected && index >= 10) {
    preselected = index;
    if(index < 16) {
      std::array<unsigned int,6> constexpr theme_profiles{0,1,2,2,4,5};
      profile = theme_profiles[index - 10];context.lapc_orders = profiles[profile];
    }
    return;
  }
  if(preselected) { index = *preselected;preselected.reset(); }
  replace_locked(index);control.last_cue = 0;control.resume_cue = 0;
  if(enabled && resource) playing = sequencer.play(*resource);
}
void music_player::stop() {
  std::lock_guard lock{mutex};
  stop_locked();
  flight_active = false;
}
void music_player::enable(bool const value) {
  std::lock_guard lock{mutex};
  if(enabled == value) return;
  enabled = context.enabled = value;
  if(!flight_active) {
    ticks_enabled = value;
    if(playing) sequencer.volume(*playing,value ? 75 : 0);
    return;
  }
  context.music_playing = value;control.previous_throttle = -1;
  bool const sampled_sb{context.sound_device == 1 || context.sound_device == 3 || context.sound_device == 4};
  if(value) {
    ticks_enabled = true;
    if(playing) sequencer.volume(*playing,127);
    stop_playing_locked();
    if(control.last_cue == 12) control.last_cue = 2;
    control.engine_playing = false;
    cue_locked(control.last_cue);
  } else {
    if(!sampled_sb) { ticks_enabled = false;if(playing) sequencer.volume(*playing,0); }
    stop_playing_locked();
  }
}
bool music_player::is_enabled() const {
  std::lock_guard lock{mutex};
  return enabled;
}
void music_player::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"music output requires complete stereo frames"};
  std::lock_guard lock{mutex};
  unsigned int frames{0};
  while(frames != stereo.size() / 2) {
    while(clock.due()) { clock.consume();if(ticks_enabled) sequencer.tick(); }
    auto const count{static_cast<unsigned int>(std::min<size_t>(clock.until_tick(),stereo.size() / 2 - frames))};
    device->render(stereo.subspan(frames * 2,count * 2));
    clock.advance(count);
    frames += count;
  }
}

} // namespace tfx::audio
