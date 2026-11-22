#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "missions/progress.h"
#include "simulation/acceleration.h"

auto main(int const argc, char const *const argv[])->int try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  auto &input{fixture.is_open() ? fixture : std::cin};
  int32_t x;
  int32_t z;
  unsigned int outside;
  unsigned int requested;
  unsigned int active;
  unsigned int idle;
  int violations;
  int exit_ticks;
  unsigned int cases{0};
  while(input >> x >> z >> outside >> violations >> exit_ticks >> requested >> active >> idle) {
    tfx::missions::setup setup;
    tfx::missions::progress progress{setup};
    progress.beyond_airspace = outside != 0;
    progress.violations = static_cast<int16_t>(violations);
    progress.exit_ticks = static_cast<int16_t>(exit_ticks);
    tfx::simulation::time_acceleration acceleration{
      .requested{requested != 0},
      .active{active != 0},
    };
    auto const event{progress.observe_airspace({
      .position{x, z},
      .acceleration_active{acceleration.active},
      .radio_idle{idle != 0},
    })};
    if(event.cancel_acceleration) {
      acceleration.cancel();
      idle = 1;
    }
    if(event.notice != tfx::missions::airspace_notice::none) idle = 0;
    std::array<int, 7> const actual{progress.beyond_airspace, progress.violations, progress.exit_ticks, acceleration.requested, acceleration.active, static_cast<int>(idle), std::to_underlying(event.notice)};
    if(fixture.is_open()) {
      for(auto const value : actual) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native airspace boundary differs"};
      }
    } else {
      for(auto const value : actual) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"invalid airspace input"};
  if(fixture.is_open()) std::cout << cases << " original airspace boundary cases agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
