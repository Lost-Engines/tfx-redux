#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_speed.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits;
  input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic speed fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int strategy;
  unsigned int stages;
  unsigned int afterburner;
  unsigned int control;
  unsigned int display;
  unsigned int brake;
  unsigned int cases{0};
  while(input >> strategy >> stages >> afterburner >> control >> display >> brake) {
    tfx::flight::aerodynamic_speed_state state{
      .target_metres_per_second{read_double(input)},
      .air_brake_fraction{read_double(input)},
      .air_brake_condition{static_cast<tfx::flight::system_condition>(brake)},
    };
    tfx::flight::aerodynamic_powerplant powerplant;
    powerplant.throttle_target = read_double(input);
    powerplant.engine_factor.x = read_double(input);
    powerplant.engine_factor.y = read_double(input);
    powerplant.fuel = read_double(input);
    uint32_t throttle;
    input >> throttle;
    powerplant.throttle = std::bit_cast<float>(throttle);
    powerplant.afterburner = static_cast<uint8_t>(afterburner);
    powerplant.afterburner_control = static_cast<uint8_t>(control);
    powerplant.afterburner_display = static_cast<uint8_t>(display);
    tfx::flight::aerodynamic_speed_context const context{
      .metres_per_second{read_double(input)},
      .mass_kg{read_double(input)},
      .forward_force{read_double(input)},
      .air_brake_drag{read_double(input)},
      .afterburner_stages{stages},
      .strategy{static_cast<tfx::flight::aerodynamic_speed_strategy>(strategy)},
    };
    auto const change{tfx::flight::update_aerodynamic_speed(state, powerplant, context)};
    std::vector<uint64_t> values;
    for(auto const value : {state.target_metres_per_second, state.air_brake_fraction, powerplant.throttle_target}) values.push_back(std::bit_cast<uint64_t>(value));
    values.push_back(std::bit_cast<uint32_t>(powerplant.throttle));
    for(auto const value : {static_cast<unsigned int>(powerplant.afterburner), static_cast<unsigned int>(powerplant.afterburner_control), static_cast<unsigned int>(powerplant.afterburner_display), static_cast<unsigned int>(state.air_brake_condition), static_cast<unsigned int>(change)}) values.push_back(value);
    if(argc == 2) {
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic speed control differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic speed cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic speed fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
