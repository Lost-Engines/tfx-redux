#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "audio/runtime.h"
#include "configuration/audio.h"

namespace {
void require(bool const value, char const *message) {
  if(!value) throw std::runtime_error{message};
}
void write(std::filesystem::path const &path, tfx::resources::bytes const &bytes) {
  std::ofstream file{path, std::ios::binary};
  file.write(reinterpret_cast<char const *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if(!file) throw std::runtime_error{"cannot write audio fixture"};
}
tfx::resources::bytes bank(tfx::audio::osl_device const device, uint8_t const note) {
  // One channel, four uncompressed rows and 256 valid orders. Each catalogue
  // entry has a distinct note, so the real sequencer identifies its theme.
  tfx::resources::bytes bytes(0x600, 0);
  auto const word{[&](unsigned int at, uint16_t value){
    bytes[at] = static_cast<uint8_t>(value);
    bytes[at + 1] = static_cast<uint8_t>(value >> 8);
  }};
  auto const pointer{[&](unsigned int at, uint16_t value){word(at, value);word(at + 2, 0);}};
  bytes[0] = 'O'; bytes[1] = 'S'; bytes[2] = 'L'; bytes[3] = '1';
  bytes[7] = std::to_underlying(device);
  word(0x26, 1);
  pointer(0x48, 0x80);
  word(0x4c, 1);
  word(0x92, 1);
  word(0x94, 256);
  std::ranges::fill(std::span{bytes}.subspan(0x96, 16), 127);
  word(0xaa, 50);
  bytes[0xac] = 1;
  word(0xce, 1);
  pointer(0x1d0, 0x200);
  word(0x202, 4);
  bytes[0x204] = note;
  return bytes;
}
class driver final : public tfx::audio::osl_device_driver {
  tfx::audio::osl_device type;
public:
  uint8_t last_note{0};
  bool active{false};
  explicit driver(tfx::audio::osl_device value) : type{value} {}
  tfx::audio::osl_device device_type() const override {return type;}
  void initialise() override {active = false;}
  void instrument(unsigned int, std::span<uint8_t const>) override {}
  void note(tfx::audio::osl_channel &, unsigned int, uint8_t value, uint8_t) override {last_note = value;active = true;}
  void extra_note(tfx::audio::osl_channel const &, unsigned int, uint8_t value) override {last_note = value;active = true;}
  void silence(unsigned int) override {active = false;}
  void program(unsigned int, uint8_t) override {}
  void pan(unsigned int, uint8_t) override {}
  void volume(tfx::audio::osl_channel &, unsigned int, uint8_t) override {}
  void pitch(tfx::audio::osl_channel const &, unsigned int) override {}
  void modulation(unsigned int, uint8_t) override {}
  void reverb(std::array<uint8_t, 3> const &) override {}
};
class device final : public tfx::audio::music_device {
public:
  ::driver callbacks;
  explicit device(tfx::audio::osl_device type) : callbacks{type} {}
  tfx::audio::osl_device_driver &driver() override {return callbacks;}
  void render(std::span<float> stereo) override {std::ranges::fill(stereo, callbacks.active ? .0625f : 0.f);}
};
}

int main() try {
  using tfx::configuration::audio_preferences;
  using tfx::configuration::music_engine;
  using tfx::configuration::resolve_audio;
  std::array<music_engine, 5> constexpr original{music_engine::none, music_engine::fm, music_engine::lapc, music_engine::fm, music_engine::scc1a};
  for(uint8_t n{0}; n != original.size(); ++n) {
    auto const inherited{resolve_audio(n, {})};
    require(inherited.engine == original[n] && inherited.cd_music == (n == 1), "legacy music selection changed");
  }
  for(auto const engine : {music_engine::none, music_engine::fm, music_engine::lapc, music_engine::scc1a, music_engine::sc55}) {
    require(tfx::configuration::parse_music_engine(tfx::configuration::music_engine_name(engine)) == engine, "engine name roundtrip failed");
    auto const inherited_cd{resolve_audio(1, {.engine{engine}})};
    require(inherited_cd.engine == engine && inherited_cd.cd_music, "changing synth disabled inherited CD music");
    auto const disabled_cd{resolve_audio(1, {.engine{engine}, .cd_music{false}})};
    require(disabled_cd.engine == engine && !disabled_cd.cd_music, "CD switch changed the synth");
  }
  auto const root{std::filesystem::temp_directory_path() / ("tfx-hybrid-audio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  std::filesystem::create_directories(root / "tunes");
  struct cleanup {
    std::filesystem::path path;
    ~cleanup() {std::error_code error;std::filesystem::remove_all(path,error);}
  } const guard{root};
  tfx::resources::bytes empty(12, 0);
  empty[0] = 4;
  write(root / "DID.DAT", empty);
  tfx::resources::bytes launcher(0x6000, 0);
  std::array<unsigned int, 3> constexpr starts{0x50f5, 0x4f6e, 0x52a0};
  std::array<tfx::audio::osl_device, 3> constexpr types{tfx::audio::osl_device::fm, tfx::audio::osl_device::lapc, tfx::audio::osl_device::sound_canvas};
  for(unsigned int d{0}; d != types.size(); ++d) {
    auto at{starts[d]};
    for(unsigned int i{0}; i != 18; ++i) {
      std::string const filename{std::string(1, static_cast<char>('a' + d)) + std::to_string(i) + ".osl"};
      std::string const name{"tunes\\" + filename};
      std::ranges::copy(name, launcher.begin() + at);
      at += static_cast<unsigned int>(name.size()) + 1;
      write(root / "tunes" / filename, bank(types[d], static_cast<uint8_t>(60 + i)));
    }
  }
  // Two seconds of independent CD PCM: +0.25 left, -0.125 right.
  tfx::resources::bytes pcm(150 * 2352, 0);
  for(size_t at{0}; at != pcm.size(); at += 4) {pcm[at + 1] = 32;pcm[at + 3] = 240;}
  write(root / "disc.bin", pcm);
  auto const cue{root / "disc.cue"};
  {std::ofstream file{cue};file << "FILE \"disc.bin\" BINARY\n TRACK 05 AUDIO\n INDEX 01 00:00:00\n";}
  tfx::resources::archive const game{root / "DID.DAT"};
  std::array<tfx::audio::music_mode, 3> constexpr modes{tfx::audio::music_mode::fm, tfx::audio::music_mode::lapc, tfx::audio::music_mode::sound_canvas};
  for(unsigned int d{0}; d != types.size(); ++d) {
    auto output{std::make_unique<device>(types[d])};
    auto *const synth{output.get()};
    auto music{std::make_unique<tfx::audio::music_player>(game, launcher, std::move(output), 44100)};
    music->configure(modes[d], 1, {});
    auto disc{std::make_unique<tfx::audio::cd_player>(cue, 44100)};
    auto *const cd{disc.get()};
    tfx::audio::runtime sound{{}, game, 44100, tfx::audio::sound_device::none, std::move(music), {}, std::move(disc), modes[d], true, {}, true};
    std::array<float, 8820> buffer{};
    sound.select_music(0);
    sound.render(buffer);
    require(synth->callbacks.active && !cd->playing(), "front end did not use the synth");
    auto const intro_note{synth->callbacks.last_note};
    sound.select_music(12);
    sound.begin_flight({});
    sound.render(buffer);
    require(cd->playing() && !synth->callbacks.active && buffer[0] == .25f && buffer[1] == -.125f, "CD music failed with a non-Sound-Blaster synth");
    sound.enable_cd_music(false);
    sound.render(buffer);
    require(!cd->playing() && synth->callbacks.active && synth->callbacks.last_note == intro_note + 12, "CD off did not restore the mission's synth theme");
    sound.enable_cd_music(true);
    sound.render(buffer);
    require(cd->playing() && !synth->callbacks.active, "CD on did not suspend the synth");
    sound.enable_music(false);
    sound.enable_cd_music(false);
    sound.render(buffer);
    require(!cd->playing() && std::ranges::all_of(buffer, [](float v){return v == 0;}), "CD switch bypassed master music mute");
    sound.enable_music(true);
    sound.render(buffer);
    require(synth->callbacks.active && synth->callbacks.last_note == intro_note + 12, "unmuting lost the mission theme");
    sound.enable_music(false);
    sound.enable_cd_music(true);
    require(!cd->playing(), "CD started while music was muted");
    sound.enable_music(true);
    sound.render(buffer);
    require(cd->playing() && !synth->callbacks.active, "unmuting CD mixed in synth music");
    sound.enable_cd_music(false);
    sound.render(buffer);
    require(synth->callbacks.active && synth->callbacks.last_note == intro_note + 12, "CD toggle while muted lost theme selection");
    sound.end_flight();
    sound.select_music(12);
    sound.begin_flight({});
    sound.render(buffer);
    require(!cd->playing() && synth->callbacks.active, "flight did not start with the saved CD-off choice");
    sound.enable_cd_music(true);
    sound.render(buffer);
    require(cd->playing() && buffer[0] == .25f, "enabling CD after a synth-only launch selected the wrong campaign track");
    sound.end_flight();
    sound.select_music(2);
    sound.render(buffer);
    require(!cd->playing() && synth->callbacks.active && synth->callbacks.last_note == intro_note + 2, "post-flight news did not return to the synth");
  }
  {
    auto disc{std::make_unique<tfx::audio::cd_player>(cue, 44100)};
    auto *const cd{disc.get()};
    tfx::audio::runtime sound{{}, game, 44100, tfx::audio::sound_device::none, {}, {}, std::move(disc), tfx::audio::music_mode::none, true, {}, false};
    sound.select_music(12);
    sound.begin_flight({});
    require(!cd->playing(), "CD-only playback ignored its disabled preference");
    sound.enable_cd_music(true);
    std::array<float, 2> buffer{};
    sound.render(buffer);
    require(cd->playing() && buffer[0] == .25f, "CD-only playback lost the campaign track without a synth");
  }
  {
    auto output{std::make_unique<device>(tfx::audio::osl_device::lapc)};
    auto *const synth{output.get()};
    auto music{std::make_unique<tfx::audio::music_player>(game, launcher, std::move(output), 44100)};
    music->configure(tfx::audio::music_mode::lapc, 2, {});
    auto disc{std::make_unique<tfx::audio::cd_player>(cue, 44100)};
    auto *const cd{disc.get()};
    tfx::audio::runtime sound{{}, game, 44100, tfx::audio::sound_device::none, std::move(music), {}, std::move(disc), tfx::audio::music_mode::lapc, true, {}, true};
    sound.select_music(12);
    sound.begin_flight({});
    require(!synth->callbacks.active, "LAPC effects bank started background music over CD");
    sound.lapc_effect(tfx::audio::lapc_effect_cue::engine_start);
    std::array<float, 8820> buffer{};
    sound.render(buffer);
    require(cd->playing() && synth->callbacks.active, "CD playback disabled LAPC effects");
  }
  std::cout << "Independent CD/synth routing, legacy defaults, theme restoration, mute, CD-only playback and LAPC effects passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
