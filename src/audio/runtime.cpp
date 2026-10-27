#include "runtime.h"
#include <algorithm>
#include <bit>
#include <iostream>
#include <utility>

namespace tfx::audio {

namespace {
std::variant<sample_voice,gus_samples> make_effects(sample_bank const &bank, unsigned int const rate,
    std::optional<gus_parameters> const &gravis, std::unique_ptr<qsound_effect> spatial) {
  if(!gravis) return std::variant<sample_voice,gus_samples>{std::in_place_type<sample_voice>,bank,rate,std::move(spatial)};
  if(spatial) throw std::invalid_argument{"QSound applies only to Sound Blaster samples"};
  std::vector<gus_sample_data> samples;
  for(unsigned int index{0}; index != bank.entries().size(); ++index) samples.push_back({
    .pcm{bank.pcm(index)},
    .rate{bank.entries()[index].rate},
    .retrigger_delay{bank.entries()[index].retrigger_delay},
  });
  return std::variant<sample_voice,gus_samples>{std::in_place_type<gus_samples>,samples,*gravis,rate};
}
}
runtime::runtime(std::span<sample_definition const> const definitions, resources::archive const &game,
    unsigned int const output_rate, sound_device const device, std::unique_ptr<music_player> music, std::optional<gus_parameters> const &gravis,
    std::unique_ptr<cd_player> cd, music_mode mode, bool music_enabled, std::unique_ptr<qsound_effect> spatial) :
    bank{definitions,game}, effects{make_effects(bank,output_rate,gravis,std::move(spatial))}, available{device != sound_device::none},
    sampled{device == sound_device::sound_blaster || device == sound_device::sound_blaster16 || device == sound_device::gravis},
    enabled{available}, music{std::move(music)}, cd{std::move(cd)}, mode{mode},
    music_on{this->music ? this->music->is_enabled() : music_enabled} {
  std::visit([&](auto &output) { output.enable(enabled && sampled); },effects);
}
bool runtime::sound_available() const { return available; }
bool runtime::music_available() const { return music != nullptr || cd != nullptr; }
bool runtime::music_enabled() const { return music_available() && music_on; }
void runtime::enable_music(bool const value) {
  music_on = value;
  cd_flight = in_flight && cd && mode == music_mode::soundblaster_cd && value;
  if(music) { music->set_cd_mode(cd_flight);music->enable(value); }
  if(!value) stop_cd();
  else if(cd_flight) play_cd(); // Native enable restarts the selected track.
}
void runtime::select_music(int16_t const index) { if(music) music->select(index); }
void runtime::stop_music() { if(music) music->stop(); }
void runtime::play_cd() {
  if(!cd || !music_on) return;
  try {
    if(cd_index < 0 || !cd->play(static_cast<unsigned int>(cd_index) + 1)) std::cerr << "CD track " << cd_index + 1 << " is unavailable.\n";
  } catch(std::exception const &error) { std::cerr << "CD playback unavailable: " << error.what() << '\n'; }
}
void runtime::select_cd(int16_t const index) {
  cd_index = index;
  play_cd();
}
void runtime::stop_cd() {
  if(cd) cd->stop();
}
void runtime::begin_flight(flight_audio_state const &state) {
  in_flight = true;
  cd_flight = cd && mode == music_mode::soundblaster_cd && music_on;
  if(cd_flight) {
    auto const selected{music ? music->preselected_index() : std::nullopt};
    cd_index = selected ? static_cast<int16_t>(*selected - 8) : 8;
  }
  if(music) { music->set_cd_mode(cd_flight);music->begin_flight(state); }
  if(cd_flight) play_cd();
}
void runtime::end_flight() {
  if(cd_flight) stop_cd();
  in_flight = cd_flight = false;
  if(music) { music->set_cd_mode(false);music->end_flight(); }
}
void runtime::update_flight(flight_audio_state const &state) { if(music) music->update_flight(state); }
void runtime::music_cue(uint8_t const cue) { if(music) music->cue(cue); }
void runtime::music_cue(music_cue_id const cue) { music_cue(std::to_underlying(cue)); }
void runtime::lapc_effect(lapc_effect_cue const cue) { if(music) music->effect(cue); }
bool runtime::sound_enabled() const { return enabled; }
void runtime::enable_sound(bool const value) { enabled = available && value;std::visit([&](auto &device) { device.enable(enabled && sampled); },effects); }
void runtime::advance(simulation::timer_ticks const elapsed) {
  std::visit([&](auto &device) { device.advance(elapsed); },effects);
  if(cd_flight) {
    // Native A5374 polls only after >300 shared timer ticks, subtracts one
    // interval and discards a remaining backlog greater than another interval.
    int16_t constexpr poll_interval{300};
    cd_poll_ticks = std::bit_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(cd_poll_ticks) + elapsed));
    if(cd_poll_ticks > poll_interval) {
      cd_poll_ticks = static_cast<int16_t>(cd_poll_ticks - poll_interval);
      if(cd_poll_ticks > poll_interval) cd_poll_ticks = 0;
      if(!cd->playing()) play_cd();
    }
  }
}
void runtime::sample(sample_priority const priority, unsigned int const id) { sequence(priority,{&id,1}); }
void runtime::sample(sample_priority const priority, sample_id const id) { sample(priority,std::to_underlying(id)); }
void runtime::sequence(sample_priority const priority, std::initializer_list<unsigned int> const ids) { sequence(priority,std::span<unsigned int const>{ids}); }
void runtime::sequence(sample_priority const priority, std::span<unsigned int const> const ids) { std::visit([&](auto &device) { device.submit(priority,ids); },effects); }
void runtime::render(std::span<float> const stereo) noexcept {
  std::ranges::fill(stereo,0);
  // Device callbacks must not propagate exceptions into the platform backend.
  try { if(music) music->render(stereo);if(cd) cd->render(stereo);std::visit([&](auto &device) { device.render(stereo); },effects); } catch(...) { std::ranges::fill(stereo,0); }
}

} // namespace tfx::audio
