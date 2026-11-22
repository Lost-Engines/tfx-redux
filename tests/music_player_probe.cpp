#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "audio/music_player.h"
#include "audio/sc55_synth.h"

namespace {

struct midi_event {
  uint64_t frame;
  std::vector<uint8_t> bytes;
  bool operator==(midi_event const &) const = default;
};
class recording_receiver final : public tfx::audio::midi_receiver {
private:
  uint64_t frame{0};

public:
  std::vector<midi_event> events;
  void short_message(uint32_t const packed) override {
    events.push_back({
      .frame{frame},
      .bytes{static_cast<uint8_t>(packed),static_cast<uint8_t>(packed >> 8),static_cast<uint8_t>(packed >> 16)}
    });
  }
  void sysex(std::span<uint8_t const> const message) override {
    events.push_back({
      .frame{frame},
      .bytes{message.begin(),message.end()}
    });
  }
  void render(std::span<float> const stereo) override {
    frame += stereo.size() / 2;
    std::ranges::fill(stereo,0);
  }
};

unsigned int constexpr rate{48000};
std::array<unsigned int,7> constexpr durations{rate * 5,rate * 3,rate * 2,rate / 2,rate * 3,rate * 3,rate * 3};
std::array<unsigned int,7> constexpr chunks{1,17,128,511,1024,4096,37};
std::vector<float> exercise(tfx::audio::music_player &player, bool const fragmented, bool const check_pcm) {
  std::vector<float> result;
  for(unsigned int phase{0}; phase != durations.size(); ++phase) {
    switch(phase) {
    case 0: player.select(0);break;
    case 1: player.enable(false);break;
    case 2: player.enable(true);break;
    case 3: player.select(10);break;
    case 4: player.select(1);break;
    case 5: player.stop();break;
    case 6: player.select(1);break;
    }
    std::vector<float> output(durations[phase] * 2);
    unsigned int done{0}, chunk{0};
    while(done != durations[phase]) {
      auto const count{fragmented ? std::min(chunks[chunk++ % chunks.size()],durations[phase] - done) : durations[phase]};
      player.render(std::span{output}.subspan(done * 2,count * 2));
      done += count;
    }
    if(!check_pcm) continue;
    result.insert(result.end(),output.begin(),output.end());
    float peak{0};
    for(auto const value : output) {
      if(!std::isfinite(value)) throw std::runtime_error{"music produced nonfinite PCM"};
      peak = std::max(peak,std::abs(value));
    }
    if((phase == 0 || phase == 2 || phase == 4 || phase == 6) && peak < .001f) throw std::runtime_error{"music did not start/resume after phase " + std::to_string(phase)};
    std::cout << "phase " << phase << ": " << durations[phase] << " frames, peak " << peak << '\n';
  }
  return result;
}

std::vector<float> exercise_flight(tfx::audio::music_player &player, bool const fragmented, bool const check_pcm) {
  std::vector<float> result;
  tfx::audio::flight_audio_state state{
    .throttle{.8f},
    .engines_running{true}
  };
  player.select(12);
  player.begin_flight(state);
  unsigned int chunk{0};
  for(unsigned int frame{0}; frame != 1000; ++frame) {
    if(frame == 100) player.cue(9);
    if(frame == 150) player.cue(10);
    if(frame == 250) player.enable(false);
    if(frame == 350) state.throttle = 1;
    if(frame == 450) state.engines_running = false;
    if(frame == 550) { state.engines_running = true;state.throttle = .6f; }
    if(frame == 600) player.enable(true);
    if(frame == 650) player.cue(10);
    if(frame == 700) player.cue(6);
    if(frame == 730) player.cue(1);
    if(frame == 750) player.cue(3);
    if(frame == 800) player.end_flight();
    if(frame == 810) player.select(1);
    if(frame == 900) player.begin_flight(state);
    player.update_flight(state);
    unsigned int constexpr frame_samples{rate / 100};
    std::array<float,frame_samples * 2> output;
    unsigned int done{0};
    while(done != frame_samples) {
      auto const count{fragmented ? std::min(chunks[chunk++ % chunks.size()],frame_samples - done) : frame_samples - done};
      player.render(std::span{output}.subspan(done * 2,count * 2));done += count;
    }
    if(check_pcm) {
      for(auto const value : output) if(!std::isfinite(value)) throw std::runtime_error{"flight music produced nonfinite PCM"};
      result.insert(result.end(),output.begin(),output.end());
    }
  }
  player.end_flight();
  return result;
}

} // namespace

int main(int const argc, char const *const argv[]) try {
  if(argc < 3 || argc > 5) throw std::runtime_error{"expected START.EXE DID.DAT [ROM_DIRECTORY [sc55|scc1a]]; omission selects FM"};
  auto const launcher{tfx::resources::read_file(argv[1])};
  tfx::resources::archive const game{std::filesystem::path{argv[2]}};
  auto const executable{tfx::resources::read_file(std::filesystem::path{argv[2]}.parent_path() / "TFX.DAT")};
  tfx::audio::lapc_order_profiles profiles;
  for(unsigned int profile{0}; profile != profiles.size(); ++profile) std::ranges::copy(tfx::resources::binary_view{executable}.slice(973476 + 0xa38c + profile * 12,12),profiles[profile].begin());
  auto const configure{[&](tfx::audio::music_player &player, tfx::audio::music_mode const mode){ player.configure(mode,3,profiles); }};
  auto const record{[&](bool const fragmented){
    auto receiver{std::make_unique<recording_receiver>()};
    auto *recording{receiver.get()};
    tfx::audio::music_player player{game,launcher,(argc == 5 ? tfx::audio::make_scc_music_device(launcher,std::move(receiver)) : tfx::audio::make_la_music_device(launcher,std::move(receiver))),rate};
    configure(player,argc == 5 ? tfx::audio::music_mode::sound_canvas : tfx::audio::music_mode::lapc);
    exercise(player,fragmented,false);
    exercise_flight(player,fragmented,false);
    return std::move(recording->events);
  }};
  auto const a{record(false)}, b{record(true)};
  if(a != b) {
    auto const mismatch{std::mismatch(a.begin(),a.end(),b.begin(),b.end())};
    throw std::runtime_error{"music callback segmentation changed event or sample timestamp at event " + std::to_string(mismatch.first - a.begin())};
  }
  if(a.size() < 100) throw std::runtime_error{"music test did not exercise the MIDI stream"};
  std::cout << a.size() << " MIDI messages and exact sample timestamps agree across callback sizes, mute/resume, retained selection, replacement and restart\n";
  // Munt's pitch timer jitter consumes the host C random generator in voice
  // processing order. Test the sequencer's timestamps exactly above; do not
  // mistake chip-level random variation for a callback scheduling fault.
  if(argc == 5) {
    auto const model{std::string_view{argv[4]} == "scc1a" ? tfx::audio::sound_canvas_model::scc1a : tfx::audio::sound_canvas_model::sc55};
    tfx::audio::music_player player{game,launcher,tfx::audio::make_scc_music_device(launcher,std::make_unique<tfx::audio::sc55_synth>(argv[3],rate,model)),rate};
    configure(player,argc == 5 ? tfx::audio::music_mode::sound_canvas : tfx::audio::music_mode::lapc);
    exercise(player,true,true);
    exercise_flight(player,true,true);
  } else if(argc == 4) {
    tfx::audio::music_player player{game,launcher,argv[3],rate};
    configure(player,argc == 5 ? tfx::audio::music_mode::sound_canvas : tfx::audio::music_mode::lapc);
    exercise(player,true,true);
    exercise_flight(player,true,true);
  } else {
    auto const render{[&](bool const fragmented){
      tfx::audio::music_player player{game,launcher,tfx::audio::make_fm_music_device(launcher,rate),rate};
      configure(player,tfx::audio::music_mode::fm);
      auto result{exercise(player,fragmented,true)};
      auto const flight{exercise_flight(player,fragmented,true)};
      result.insert(result.end(),flight.begin(),flight.end());
      return result;
    }};
    auto const whole{render(false)}, segmented{render(true)};
    if(whole != segmented) throw std::runtime_error{"FM PCM changed across callback boundaries"};
    for(size_t index{0}; index != whole.size(); index += 2) if(whole[index] != whole[index + 1]) throw std::runtime_error{"FM dual mono changed"};
    std::cout << whole.size() << " FM PCM samples agree exactly across callback sizes and control transitions\n";
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
