#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include "audio/cd_player.h"
#include "audio/osl_fm_driver.h"
#include "audio/runtime.h"
#include "resources/cd_image.h"

namespace {

// This probe checks CD control, not FM hardware. Retain the real OSL driver,
// but terminate its register writes at the same explicit transport boundary
// as MSCDEX in the independent original-program comparison.
class control_device final : public tfx::audio::music_device {
private:
  tfx::audio::osl_fm_driver callbacks;

public:
  explicit control_device(std::span<uint8_t const> const launcher) :
    callbacks{launcher, [](tfx::audio::opl_write){}} {
  }
  tfx::audio::osl_device_driver &driver() override {
    return callbacks;
  }
  void render(std::span<float> const stereo) override {
    std::ranges::fill(stereo, 0);
  }
};

} // anonymous namespace

int main(int argc, char const *const argv[]) try {
  if(argc == 2 && std::string_view{argv[1]} == "self-test") {
    auto const root{std::filesystem::temp_directory_path() / ("tfx-cd-unit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    std::filesystem::create_directory(root);
    struct cleanup {
      std::filesystem::path const &root;
      ~cleanup() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
      }
    } const guard{
      .root{root},
    };
    unsigned int constexpr sectors{6};
    unsigned int constexpr frames{sectors * 588};
    std::vector<uint8_t> pcm(frames * 4);
    for(unsigned int i{0}; i != frames; ++i) {
      pcm[i * 4] = 0;
      pcm[i * 4 + 1] = 32;                                                     // +8192 / 32768
      pcm[i * 4 + 2] = 0;
      pcm[i * 4 + 3] = 240;                                                    // -4096 / 32768
    }
    {
      std::ofstream file{root / "audio.bin", std::ios::binary};
      std::array<uint8_t, 2352> pregap{};
      pregap.fill(127);
      file.write(reinterpret_cast<char const *>(pregap.data()), pregap.size());
      file.write(reinterpret_cast<char const *>(pcm.data()), static_cast<std::streamsize>(pcm.size()));
    }
    auto const cue{root / "test.cue"};
    {
      std::ofstream file{cue};
      file << "FILE \"audio.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:00:01\n";
    }
    tfx::resources::cd_image const image{cue};
    if(image.track(2).file_sector != 1 || image.track(2).sectors != sectors || image.track(2).disc_sector != 1) throw std::runtime_error{"CUE extent/pregap changed"};
    auto const check{[&](std::filesystem::path const &source){
      tfx::audio::cd_player player{source, 44100};
      if(player.play(1) || !player.play(2) || !player.playing()) throw std::runtime_error{"CD physical track selection failed"};
      std::vector<float> output((frames + 64) * 2, 0);
      player.render(output);
      for(unsigned int i{0}; i != frames; ++i) {
        if(output[i * 2] != 0.25f || output[i * 2 + 1] != -.125f) throw std::runtime_error{"CD PCM includes a pregap or changed amplitude/channel order"};
      }
      if(std::ranges::any_of(std::span{output}.subspan(frames * 2), [](float value){return value != 0;}) || player.playing()) throw std::runtime_error{"CD track exceeded its end"};
      if(!player.play(2)) throw std::runtime_error{"CD restart failed"};
      player.stop();
      if(player.playing()) throw std::runtime_error{"CD stop failed"};
    }};
    check(cue);
    // Independent ordinary PCM WAV fixture: 44.1 kHz, 16-bit stereo, 14112 bytes.
    std::array<uint8_t, 44> constexpr header{82, 73, 70, 70, 68, 55, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32, 16, 0, 0, 0, 1, 0, 2, 0, 68, 172, 0, 0, 16, 177, 2, 0, 4, 0, 16, 0, 100, 97, 116, 97, 32, 55, 0, 0};
    auto const rip{root / "ripped"};
    std::filesystem::create_directory(rip);
    {
      std::ofstream file{rip / "TrAcK02.WAV", std::ios::binary};
      file.write(reinterpret_cast<char const *>(header.data()), header.size());
      file.write(reinterpret_cast<char const *>(pcm.data()), static_cast<std::streamsize>(pcm.size()));
    }
    check(rip);
    std::cout << "CUE extents, stereo PCM, numbered local WAV, exact EOF, restart and stop passed\n";
    return EXIT_SUCCESS;
  }
  if(argc < 3) return 2;
  std::string const command{argv[1]};
  if(command == "control" && argc == 4) {
    tfx::resources::bytes empty(12, 0);
    empty[0] = 4;
    tfx::resources::archive const game{std::move(empty)};
    auto const launcher{tfx::resources::read_file(argv[3])};
    auto music{std::make_unique<tfx::audio::music_player>(game, launcher, std::make_unique<control_device>(launcher), 44100)};
    music->configure(tfx::audio::music_mode::soundblaster_cd, 1, {});
    auto cd{std::make_unique<tfx::audio::cd_player>(argv[2], 44100)};
    auto const *observer{cd.get()};
    tfx::audio::runtime sound{{}, game, 44100, tfx::audio::sound_device::none, std::move(music), {}, std::move(cd), tfx::audio::music_mode::soundblaster_cd, true};
    std::string action;
    unsigned int value{0};
    while(std::cin >> action >> value) {
      if(action == "begin") {
        if(value) sound.select_music(static_cast<int16_t>(value));
        sound.begin_flight({});
      } else if(action == "advance") {
        sound.advance(static_cast<uint16_t>(value));
      } else if(action == "enable") {
        sound.enable_music(value != 0);
      } else if(action == "cue") {
        sound.music_cue(static_cast<uint8_t>(value));
      } else if(action == "end") {
        sound.end_flight();
      } else if(action == "play") {
        sound.select_cd(static_cast<int16_t>(value));
      } else if(action == "stop") {
        sound.stop_cd();
      } else if(action == "drain") {
        std::array<float, 4096> pcm{};
        sound.render(pcm);
      } else {
        throw std::runtime_error{"invalid control action"};
      }
      std::array<float, 2> pcm{};
      sound.render(pcm);
      std::cout << std::lround(pcm[0] * 32768 / 1000) << ' ' << observer->playing() << '\n';
    }
  } else if(command == "catalog") {
    tfx::resources::cd_image const image{argv[2]};
    for(auto const &track : image.tracks()) std::cout << track.number << ' ' << track.audio << ' ' << track.file_sector << ' ' << track.sectors << ' ' << track.disc_sector << '\n';
  } else if(command == "install" && argc == 4) {
    tfx::resources::install_reference_cd(argv[2], argv[3]);
  } else if(command == "read" && argc == 5) {
    tfx::resources::cd_image const image{argv[2]};
    auto const bytes{image.read_file(argv[3])};
    std::ofstream output{argv[4], std::ios::binary};
    output.write(reinterpret_cast<char const *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  } else if(command == "render" && argc == 7) {
    auto const track{static_cast<unsigned int>(std::stoul(argv[3]))};
    auto const rate{static_cast<unsigned int>(std::stoul(argv[4]))};
    auto const frames{static_cast<unsigned int>(std::stoul(argv[5]))};
    tfx::audio::cd_player player{argv[2], rate};
    if(!player.play(track)) throw std::runtime_error{"CD track missing or not audio"};
    std::ofstream output{argv[6], std::ios::binary};
    std::array<float, 256 * 2> buffer{};
    for(unsigned int at{0}; at < frames;) {
      unsigned int const count{std::min(256u, frames - at)};
      std::ranges::fill(buffer, 0);
      player.render(std::span{buffer}.first(count * 2));
      output.write(reinterpret_cast<char const *>(buffer.data()), count * 2 * sizeof(float));
      at += count;
    }
    std::cout << "playing " << player.playing() << '\n';
    player.stop();
    if(player.playing()) throw std::runtime_error{"CD did not stop"};
  } else {
    return 2;
  }
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
