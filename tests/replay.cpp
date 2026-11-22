#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include "simulation/timing.h"
#include "world/animation.h"

auto main()->int try {
  // Whitespace trace: initial counter; then model, warp, allowed, poll count, counters.
  // Each completed frame reports raw numerical bits and ordered projectile substeps.
  uint32_t initial{0};
  if(!(std::cin >> initial)) throw std::runtime_error{"replay requires an initial timer counter"};
  tfx::simulation::flight_clock clock{initial};
  tfx::world::animation_clock scenery;
  tfx::simulation::timer_ticks previous{5};                                    // original 0x72018 seeds the first animation interval
  unsigned int model{0};
  unsigned int warp{0};
  unsigned int allowed{0};
  unsigned int polls{0};
  while(std::cin >> model >> warp >> allowed >> polls) {
    bool const animate{scenery.advance(previous)};                             // original scenery gate precedes the current player update
    clock.begin({
      .model{tfx::simulation::configured_flight_model(model)},
      .time_warp{warp != 0},
      .time_warp_allowed{allowed != 0},
    });
    std::optional<tfx::simulation::frame_timing> frame;
    for(unsigned int i{0}; i != polls; ++i) {
      uint32_t counter{0};
      if(!(std::cin >> counter)) throw std::runtime_error{"truncated replay timer inputs"};
      if(frame) throw std::runtime_error{"replay supplied timer readings beyond a completed frame"};
      frame = clock.poll(counter);
    }
    if(!frame) throw std::runtime_error{"replay frame has not accumulated enough time"};
    previous = frame->world_ticks;
    std::cout << previous << ' ' << std::bit_cast<uint32_t>(frame->filtered_ticks) << ' ' << std::bit_cast<uint64_t>(frame->physics_seconds) << ' ' << animate << ' ' << scenery.remainder();
    tfx::simulation::projectile_clock projectiles{previous};
    while(auto const step{projectiles.next()}) std::cout << ' ' << *step;
    std::cout << '\n';
  }
  if(!std::cin.eof()) throw std::runtime_error{"invalid replay frame"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
