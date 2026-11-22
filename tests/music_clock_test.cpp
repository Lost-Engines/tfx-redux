#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>
#include "audio/music_clock.h"

namespace {
std::vector<unsigned int> interrupts(unsigned int const rate, std::span<unsigned int const> const chunks) {
  tfx::audio::music_clock clock{rate};
  unsigned int frame{0}, chunk{0};
  std::vector<unsigned int> result;
  while(frame < rate * 10u) {
    auto remaining{std::min(chunks[chunk++ % chunks.size()],rate * 10u - frame)};
    while(remaining) {
      while(clock.due()) { clock.consume();result.push_back(frame); }
      auto const count{std::min(remaining,clock.until_tick())};
      clock.advance(count);remaining -= count;frame += count;
    }
  }
  while(clock.due()) { clock.consume();result.push_back(frame); }
  return result;
}
} // anonymous namespace
int main() try {
  std::array<unsigned int,1> constexpr whole{1000000};
  std::array<unsigned int,6> constexpr split{1,7,64,128,511,4096};
  for(unsigned int rate : {8000u,44100u,48000u,96000u}) {
    auto const a{interrupts(rate,whole)}, b{interrupts(rate,split)};
    if(a != b || a.size() != 10ull * tfx::simulation::pit_input_hz / tfx::simulation::pit_divisor) throw std::runtime_error{"shared music IRQ depends on PCM callback size"};
    for(unsigned int index{0}; index != a.size(); ++index) {
      uint64_t const cycles{static_cast<uint64_t>(index + 1) * tfx::simulation::pit_divisor};
      if(a[index] != (cycles * rate + tfx::simulation::pit_input_hz - 1) / tfx::simulation::pit_input_hz) throw std::runtime_error{"music IRQ sample phase drifted"};
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
