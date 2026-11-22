#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "flight/player_dispatch.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected [FILE]"};
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc == 2 && !fixture) throw std::runtime_error{"cannot open player dispatch fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int model,elapsed,limit,autopilot,landing,ejected,falling,cases{0};
  while(input >> model >> elapsed >> limit >> autopilot >> landing >> ejected >> falling) {
    auto const result{tfx::flight::select_player_callback({
      .model{static_cast<tfx::simulation::flight_model>(model)},
      .elapsed{static_cast<tfx::simulation::timer_ticks>(elapsed)},
      .autopilot_military_tick_limit{std::bit_cast<float>(limit)},
      .autopilot{autopilot != 0},
      .automatic_landing{landing != 0},
      .aircraft_disabled{ejected != 0},
      .pilot_active{falling != 0},
    })};
    std::array<unsigned int,3> const values{static_cast<unsigned int>(result.callback),static_cast<unsigned int>(!result.clear_autopilot && autopilot),static_cast<unsigned int>(!result.clear_automatic_landing && landing)};
    for(auto const value : values) {
      if(argc == 2) {
        unsigned int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native player dispatch differs"};
      } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no player dispatch cases"};
  if(argc == 2) std::cout << cases << " native player dispatch fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
