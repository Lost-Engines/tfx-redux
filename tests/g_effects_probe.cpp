#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "render/g_effects.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::render::g_effects_state state;
  unsigned int load,realism,elapsed,reset,strain,mode,cases{0};
  int black,red;
  while(input >> load >> realism >> elapsed >> reset >> strain >> mode >> black >> red) {
    if(reset) state = {
      .strain{std::bit_cast<float>(strain)},
      .mode{static_cast<tfx::render::blackout_mode>(mode)},
      .blackout_level{static_cast<int16_t>(black)},
      .redout_level{static_cast<int16_t>(red)},
    };
    tfx::render::advance_g_effects(state,std::bit_cast<float>(load),static_cast<tfx::configuration::g_effects>(realism),static_cast<tfx::simulation::timer_ticks>(elapsed));
    std::array<int64_t,4> const values{std::bit_cast<uint32_t>(state.strain),static_cast<int64_t>(state.mode),state.blackout_level,state.redout_level};
    for(auto const value : values) {
      if(argc == 2) { int64_t expected; if(!(input >> expected) || expected != value) throw std::runtime_error{"G effect mismatch in case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
