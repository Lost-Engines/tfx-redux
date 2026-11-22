#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "audio/fm_synth.h"

int main() try {
  for(unsigned int const rate : {44100u,48000u}) {
    auto const render{[&](bool const fragmented){
      tfx::audio::fm_synth synth{rate};
      for(auto const address : {0x20u,0x23u}) synth.write({
        .address{static_cast<uint8_t>(address)},
        .value{1},
      });
      for(auto const address : {0x60u,0x63u}) synth.write({
        .address{static_cast<uint8_t>(address)},
        .value{0xf0},
      });
      synth.write({
        .address{0xa0},
        .value{0x98},
      });
      synth.write({
        .address{0xb0},
        .value{0x31},
      });
      std::vector<float> pcm(rate * 2u);
      std::array<unsigned int,6> constexpr chunks{1,17,511,37,1024,4096};
      unsigned int at{0}, chunk{0};
      while(at != rate) {
        auto const count{fragmented ? std::min(chunks[chunk++ % chunks.size()],rate - at) : rate};
        synth.render(std::span{pcm}.subspan(at * 2u,count * 2u));
        at += count;
      }
      return pcm;
    }};
    auto const whole{render(false)}, fragmented{render(true)};
    if(whole != fragmented) throw std::runtime_error{"FM callback partition changed PCM"};
    if(std::ranges::none_of(whole,[](float const value){ return std::abs(value) > .001f; })) throw std::runtime_error{"FM fixture produced silence"};
    for(unsigned int frame{0}; frame != rate; ++frame) {
      if(!std::isfinite(whole[frame * 2u]) || whole[frame * 2u] != whole[frame * 2u + 1u]) throw std::runtime_error{"FM output lost finite dual-mono samples"};
    }
  }
  std::cout << "FM initialization and native/resampled PCM agree across callback partitions\n";
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
