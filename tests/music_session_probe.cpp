#include <algorithm>
#include <cstdlib>
#include <iostream>
#include "audio/music_player.h"
#include "audio/osl_fm_driver.h"

namespace {
class fm_device final : public tfx::audio::music_device {
private:
  uint64_t frame{0};
  tfx::audio::osl_fm_driver callbacks;
public:
  explicit fm_device(std::span<uint8_t const> const launcher) : callbacks{launcher, [this](tfx::audio::opl_write const value){
    std::cout << frame << " 2 " << static_cast<unsigned int>(value.address) << ' ' << static_cast<unsigned int>(value.value) << '\n';
  }} {}
  tfx::audio::osl_device_driver &driver() override {
    return callbacks;
  }
  void render(std::span<float> const stereo) override {
    frame += stereo.size() / 2;
    std::ranges::fill(stereo, 0);
  }
};
class receiver final : public tfx::audio::midi_receiver {
private:
  uint64_t frame{0};
  void message(std::span<uint8_t const> const bytes) {
    std::cout << frame << ' ' << bytes.size();
    for(auto const byte : bytes) std::cout << ' ' << static_cast<unsigned int>(byte);
    std::cout << '\n';
  }
public:
  void short_message(uint32_t const packed) override {
    std::array<uint8_t, 3> const bytes{static_cast<uint8_t>(packed), static_cast<uint8_t>(packed >> 8), static_cast<uint8_t>(packed >> 16)};
    message(bytes);
  }
  void sysex(std::span<uint8_t const> const bytes) override {
    message(bytes);
  }
  void render(std::span<float> const stereo) override {
    frame += stereo.size() / 2;
    std::ranges::fill(stereo, 0);
  }
};
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc != 4 && argc != 5) throw std::runtime_error{"expected START.EXE DID.DAT lapc|scc|fm|lapc-effects [CYCLES]"};
  unsigned int const cycles{argc == 5 ? static_cast<unsigned int>(std::stoul(argv[4])) : 1};
  if(!cycles || cycles > 1024) throw std::runtime_error{"cycles must be between 1 and 1024"};
  bool const scc{std::string_view{argv[3]} == "scc"};
  bool const fm{std::string_view{argv[3]} == "fm"};
  bool const effects{std::string_view{argv[3]} == "lapc-effects"};
  auto const launcher{tfx::resources::read_file(argv[1])};
  tfx::resources::archive const game{std::filesystem::path{argv[2]}};
  auto const executable{tfx::resources::read_file(std::filesystem::path{argv[2]}.parent_path() / "TFX.DAT")};
  tfx::audio::lapc_order_profiles profiles;
  for(unsigned int profile{0}; profile != profiles.size(); ++profile) std::ranges::copy(tfx::resources::binary_view{executable}.slice(973476 + 0xa38c + profile * 12, 12), profiles[profile].begin());
  std::unique_ptr<tfx::audio::music_device> hardware;
  if(fm) {
    hardware = std::make_unique<fm_device>(launcher);
  } else {
    auto output{std::make_unique<receiver>()};
    hardware = scc ? tfx::audio::make_scc_music_device(launcher, std::move(output)) : tfx::audio::make_la_music_device(launcher, std::move(output));
  }
  tfx::audio::music_player player{game, launcher, std::move(hardware), 48000};
  player.configure(scc ? tfx::audio::music_mode::sound_canvas : fm ? tfx::audio::music_mode::fm : tfx::audio::music_mode::lapc, effects ? 2 : 3, profiles);
  tfx::audio::flight_audio_state state{
    .throttle{0.8f},
    .engines_running{true}
  };
  if(effects) {
    for(unsigned int cycle{0}; cycle != cycles; ++cycle) {
      player.begin_flight(state);
      std::array<float, 960> pcm;
      std::array<tfx::audio::lapc_effect_cue, 8> constexpr cues{
        tfx::audio::lapc_effect_cue::engine_start, tfx::audio::lapc_effect_cue::afterburner_start,
        tfx::audio::lapc_effect_cue::engine_stop, tfx::audio::lapc_effect_cue::fly_past,
        tfx::audio::lapc_effect_cue::mechanical_switch, tfx::audio::lapc_effect_cue::missile_release,
        tfx::audio::lapc_effect_cue::bomb_release, tfx::audio::lapc_effect_cue::cannon_fire,
      };
      for(unsigned int frame{0}; frame != 1000; ++frame) {
        if(frame == 150) player.enable(false);
        if(frame == 250) player.enable(true);
        if(frame == 400) player.end_flight();
        if(frame == 500) player.begin_flight(state);
        if(frame < 12 || frame % 17 == 0) player.effect(cues[frame % cues.size()]);
        player.update_flight(state);
        player.render(pcm);
      }
      player.end_flight();
    }
    return EXIT_SUCCESS;
  }
  std::array<float, 960> pcm;
  for(unsigned int cycle{0}; cycle != cycles; ++cycle) {
    state.throttle = 0.8f;
    state.engines_running = true;
    player.select(12);
    player.begin_flight(state);
    for(unsigned int frame{0}; frame != 1000; ++frame) {
      if(frame == 100) player.cue(9);
      if(frame == 150) player.cue(10);
      if(frame == 250) player.enable(false);
      if(frame == 350) state.throttle = 1;
      if(frame == 450) state.engines_running = false;
      if(frame == 550) {
        state.engines_running = true;
        state.throttle = 0.6f;
      }
      if(frame == 600) player.enable(true);
      if(frame == 650) player.cue(10);
      if(frame == 700) player.cue(6);
      if(frame == 730) player.cue(1);
      if(frame == 750) player.cue(3);
      if(frame == 800) player.end_flight();
      if(frame == 810) player.select(1);
      if(frame == 900) player.begin_flight(state);
      player.update_flight(state);
      player.render(pcm);
    }
    player.end_flight();
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
