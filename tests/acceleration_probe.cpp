#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "cockpit/radio_ticker.h"
#include "simulation/acceleration.h"

auto main(int const argc, char const *const argv[])->int try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  auto &input{fixture.is_open() ? fixture : std::cin};
  unsigned int requested;
  unsigned int toggle;
  unsigned int steering;
  unsigned int autopilot;
  unsigned int mode;
  unsigned int persistent;
  unsigned int cases{0};
  while(input >> requested >> toggle >> steering >> autopilot >> mode >> persistent) {
    tfx::simulation::time_acceleration acceleration{
      .requested{requested != 0},
    };
    tfx::cockpit::radio_ticker radio{"                    "};
    radio.show("PREVIOUS MESSAGE", persistent != 0);
    if(toggle) {
      if(acceleration.toggle()) {
        radio.show("TIME WARP    ", true);
      } else {
        radio.stop();
      }
    }
    acceleration.resolve(steering != 0, autopilot && mode == 4);
    auto const state{radio.status()};
    std::array<unsigned int, 5> const actual{acceleration.requested, acceleration.active, state.complete, state.history, state.persistent};
    if(fixture.is_open()) {
      for(auto const value : actual) {
        unsigned int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native acceleration control differs"};
      }
    } else {
      for(auto const value : actual) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"invalid acceleration input"};
  if(fixture.is_open()) std::cout << cases << " original accelerated-time control cases agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
