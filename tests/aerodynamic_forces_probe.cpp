#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_forces.h"

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
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic force fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int connected;
  unsigned int arrested;
  unsigned int cases{0};
  while(input >> connected >> arrested) {
    tfx::flight::aerodynamic_motion state;
    tfx::flight::aerodynamic_force_context context{
      .refuelling_connected{connected != 0},
      .fully_arrested{arrested != 0},
    };
    for(auto *vector : {&context.basis.forward, &context.basis.right, &context.basis.down, &context.loads.aerodynamic_force, &context.loads.engine_force, &context.loads.ground_force, &context.loads.aerodynamic_moment, &context.loads.engine_moment, &context.loads.ground_moment, &state.velocity_ned, &state.ground_velocity_body, &state.air_velocity_body}) {
      for(auto &value : *vector) value = read_double(input);
    }
    context.inertia.mass_kg = read_double(input);
    context.inertia.roll_moment = read_double(input);
    context.inertia.pitch_moment = read_double(input);
    context.inertia.fuel_kg = read_double(input);
    context.cockpit_upward_metres = read_double(input);
    state.angular_rate.x = read_double(input);
    auto result{tfx::flight::update_aerodynamic_forces(state, context)};
    std::vector<uint64_t> values;
    for(auto const *vector : {&result.body_force, &result.ned_force, &result.body_moment, &state.velocity_ned, &state.ground_velocity_body, &state.air_velocity_body, &state.acceleration_ned}) {
      for(auto const value : *vector) values.push_back(std::bit_cast<uint64_t>(value));
    }
    for(auto const value : state.angular_acceleration) values.push_back(std::bit_cast<uint64_t>(value));
    values.push_back(std::bit_cast<uint32_t>(result.g_load));
    if(argc == 2) {
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic forces differ"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic force cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic force fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
