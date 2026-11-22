#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "flight/player_frame.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int elapsed,terrain,landed,cases{0};
  int height,down,collision,before,burn,after;
  while(input >> elapsed >> terrain >> landed >> height >> down >> collision >> before >> burn >> after) {
    tfx::flight::aerodynamic_flight flight;
    tfx::flight::damage_state damage;
    tfx::flight::fuel_gauge fuel{static_cast<int16_t>(before),static_cast<int16_t>(burn)};
    vec3f previous;
    for(auto *vector : {&previous,&flight.view.position}) for(auto &value : *vector) {
      uint32_t bits; input >> bits; value = std::bit_cast<float>(bits);
    }
    for(auto &value : flight.powerplant.engine_factor) {
      uint64_t bits; input >> bits; value = std::bit_cast<double>(bits);
    }
    flight.ground.ground.landed = landed != 0;
    flight.view.orientation.down.y = down;
    flight.response.motion.collision = static_cast<int16_t>(collision);
    flight.powerplant.fuel_readout = static_cast<int16_t>(after);
    flight.autopilot.recovery.model_reset_requested = true;
    auto const speed{tfx::flight::finish_player_frame(flight,damage,fuel,{
      .previous_position{previous},
      .elapsed{static_cast<tfx::simulation::timer_ticks>(elapsed)},
      .ground_elevation_steps{static_cast<int16_t>(height)},
      .terrain_present{terrain != 0},
    })};
    std::array<int64_t,8> const values{std::bit_cast<uint32_t>(flight.view.position.y),flight.response.motion.collision,static_cast<int64_t>(flight.powerplant.engine_condition[0]),static_cast<int64_t>(flight.powerplant.engine_condition[1]),fuel.reading,fuel.smoothed_burn,speed,flight.autopilot.recovery.model_reset_requested};
    for(auto const value : values) {
      if(argc == 2) { int64_t expected; if(!(input >> expected) || expected != value) throw std::runtime_error{"player frame mismatch in case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
