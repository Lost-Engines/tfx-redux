#include <array>
#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "flight/aerodynamic.h"

auto main(int const argc, char const *const argv[])->int try {
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open flight-configuration fixture"};
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  unsigned int craft;
  unsigned int model;
  unsigned int configuration;
  unsigned int cases{0};
  while(input >> craft >> model >> configuration) {
    tfx::flight::aerodynamic_flight state;
    state.aircraft.stores_and_mounts.stores.aircraft = static_cast<tfx::flight::player_aircraft>(craft);
    state.model = static_cast<tfx::flight::aerodynamic_model>(model);
    state.configure_controls(static_cast<tfx::flight::input_configuration>(configuration));
    std::array<uint64_t, 5> const actual{
      state.autopilot.recovery.configuration,
      std::bit_cast<uint64_t>(state.input_gains.joystick),
      std::bit_cast<uint64_t>(state.input_gains.pitch),
      std::bit_cast<uint64_t>(state.input_gains.roll),
      std::bit_cast<uint64_t>(state.input_gains.rudder),
    };
    if(fixture.is_open()) {
      std::array<uint64_t, 5> expected;
      for(auto &value : expected) {
        if(!(input >> value)) throw std::runtime_error{"truncated flight-configuration fixture"};
      }
      if(actual != expected) throw std::runtime_error{"native flight-configuration mismatch"};
    } else {
      for(auto const value : actual) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no flight-configuration cases"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
