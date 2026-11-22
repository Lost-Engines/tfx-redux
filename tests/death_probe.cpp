#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#include "audio/requests.h"
#include "missions/progress.h"

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2};
  std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read player-loss fixture"};
  unsigned int ejected{0};
  unsigned int impact{0};
  unsigned int fatal{0};
  unsigned int elapsed{0};
  unsigned int invulnerable{0};
  unsigned int pilot{0};
  unsigned int contact{0};
  int ticks{0};
  int collision{0};
  int32_t height{0};
  tfx::missions::setup setup;
  while(input >> ejected >> impact >> ticks >> fatal >> collision >> elapsed >> invulnerable >> height >> pilot >> contact) {
    tfx::missions::progress state{setup};
    state.player_ejected = ejected != 0;
    state.impact_started = impact != 0;
    state.death_ticks = static_cast<int16_t>(ticks);
    auto const effects{state.observe_player_loss({
      .fatal{fatal != 0},
      .collision{collision != 0},
      .elapsed{static_cast<uint16_t>(elapsed)},
      .invulnerable{invulnerable != 0},
      .height{height},
      .pilot_active{pilot != 0},
      .contact_explosion{contact != 0},
    })};
    std::vector<int64_t> values{state.player_ejected, state.impact_started, state.death_ticks, state.ended, std::to_underlying(state.outcome), std::to_underlying(state.status), effects.first_loss, effects.first_impact, effects.explosion};
    if(effects.explosion) {
      values.insert(values.end(), {1, effects.mayday ? 2 : 1, std::to_underlying(tfx::audio::sample_id::aircraft_explosion)});
      if(effects.mayday) values.push_back(std::to_underlying(tfx::audio::sample_id::mayday));
    }
    values.push_back(effects.force_collision ? 1 : collision);
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) {
      for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    }
    if(check) {
      uint64_t expected{0};
      if(!(input >> expected) || expected != digest) throw std::runtime_error{"native player-loss transition differs"};
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
