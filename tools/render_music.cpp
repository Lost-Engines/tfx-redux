// Offline first-traversal auditions using the game's actual sequencer and synths.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include "audio/la_synth.h"
#include "audio/music_clock.h"
#include "audio/music_device.h"
#include "audio/osl_sequencer.h"
#include "audio/sc55_synth.h"

namespace {

unsigned int constexpr sample_rate{48000};
uint64_t constexpr hash_basis{14695981039346656037ull}, hash_prime{1099511628211ull};

void state_hash(uint64_t &hash, tfx::audio::osl_sequencer const &sequencer, tfx::audio::osl_playing_id const playing) {
  auto const &state{sequencer.state(playing)};
  for(unsigned int const value : std::initializer_list<unsigned int>{state.flags,state.channel_count,state.speed,state.clock_rate,state.subtick,state.order,state.loop_order,state.volume,state.row,sequencer.pattern_offset(playing),state.order_count,state.reverb[0],state.reverb[1],state.reverb[2],state.mask,sequencer.clock_rate()}) {
    for(unsigned int shift{0}; shift != 32; shift += 8) hash = (hash ^ static_cast<uint8_t>(value >> shift)) * hash_prime;
  }
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc != 8) throw std::runtime_error{"usage: music_render START.EXE OSL_FILE PCM_FILE ROM_DIRECTORY adl|rld|scc|sc55 BOUNDARY_TICK include|exclude"};
  auto const launcher{tfx::resources::read_file(argv[1])};
  auto source{tfx::resources::read_file(argv[2])};
  std::string_view const mode{argv[5]};
  auto const boundary{std::stoul(argv[6])};
  if(!boundary || boundary > 60000) throw std::runtime_error{"boundary tick must be between 1 and 60000"};
  std::string_view const policy{argv[7]};
  if(policy != "include" && policy != "exclude") throw std::runtime_error{"boundary policy must be include or exclude"};
  std::unique_ptr<tfx::audio::music_device> device;
  if(mode == "adl") device = tfx::audio::make_fm_music_device(launcher,sample_rate);
  else if(mode == "rld") device = tfx::audio::make_la_music_device(launcher,std::make_unique<tfx::audio::la_synth>(argv[4],sample_rate));
  else if(mode == "scc" || mode == "sc55") device = tfx::audio::make_scc_music_device(launcher,std::make_unique<tfx::audio::sc55_synth>(argv[4],sample_rate,mode == "sc55" ? tfx::audio::sound_canvas_model::sc55 : tfx::audio::sound_canvas_model::scc1a));
  else throw std::runtime_error{"unknown synthesiser"};
  auto const expected_device{mode == "adl" ? tfx::audio::osl_device::fm : mode == "rld" ? tfx::audio::osl_device::lapc : tfx::audio::osl_device::sound_canvas};
  if(source.at(7) != std::to_underlying(expected_device)) throw std::runtime_error{"resource and synthesiser do not match"};
  tfx::audio::osl_sequencer sequencer;
  sequencer.register_device(device->driver());
  device->driver().initialise();
  std::vector<float> settling(device->reset_settling_frames(sample_rate) * 2u);
  device->render(settling);
  auto const resource{sequencer.load(std::move(source))};
  auto const playing{sequencer.play(resource,0,0,0,75)};
  uint64_t hash{hash_basis};
  state_hash(hash,sequencer,playing);
  std::ofstream output{argv[3],std::ios::binary};
  if(!output) throw std::runtime_error{"cannot create PCM output"};
  std::array<float,2048> samples;
  uint64_t frames{0};
  double peak{0};
  double energy{0};
  auto const render{[&](uint64_t count) {
    while(count) {
      auto const size{static_cast<unsigned int>(std::min<uint64_t>(count,samples.size() / 2))};
      auto const buffer{std::span{samples}.first(size * 2u)};
      device->render(buffer);
      for(float const value : buffer) {
        if(!std::isfinite(value)) throw std::runtime_error{"synthesiser produced non-finite PCM"};
        peak = std::max(peak,static_cast<double>(std::abs(value)));
        energy += static_cast<double>(value) * value;
      }
      output.write(reinterpret_cast<char const*>(buffer.data()),static_cast<std::streamsize>(buffer.size_bytes()));
      if(!output) throw std::runtime_error{"cannot write PCM output"};
      count -= size;
      frames += size;
    }
  }};
  tfx::audio::music_clock clock{sample_rate};
  for(unsigned int tick{1}; tick <= boundary; ++tick) {
    auto const count{clock.until_tick()};
    render(count);clock.advance(count);clock.consume();
    if(tick != boundary || policy == "include") { sequencer.tick();state_hash(hash,sequencer,playing); }
  }
  auto const native_end{frames};
  // Stop through the real device driver, preserving instrument release/reverb.
  // A backwards jump's first repeated notes are excluded from this traversal.
  sequencer.stop(playing);
  render(sample_rate * 2u);
  output.close();
  std::cout << std::format("TFX_RENDER {{\"frames\":{},\"boundary_frame\":{},\"ticks\":{},\"rendered_ticks\":{},\"state_trace_fnv64\":\"{:016x}\",\"raw_peak\":{:.12g},\"rms\":{:.12g},\"sample_rate\":{},\"song_volume\":75}}\n",frames,native_end,boundary,boundary - (policy == "exclude"),hash,peak,static_cast<double>(std::sqrt(energy / (static_cast<double>(frames) * 2))),sample_rate);
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
