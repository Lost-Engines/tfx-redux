#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/automatic_approach.h"

namespace {
double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected [FILE]"};
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc == 2 && !fixture) throw std::runtime_error{"cannot open automatic approach fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int heading,carrier,flare,cases{0};
  while(input >> heading >> carrier >> flare) {
    tfx::flight::automatic_approach state;
    state.flaring = flare != 0;
    for(auto *vector : {&state.destination_metres,&state.player_metres}) for(auto &value : *vector) input >> value;
    tfx::flight::aerodynamic_motion motion;
    for(auto &value : motion.pose.metres_ned) value = read_double(input);
    for(auto &value : motion.rotation_remainder) value = read_double(input);
    state.distance_km = read_double(input);
    state.pitch_degrees = read_double(input);
    double speed{read_double(input)};
    tfx::flight::automatic_approach_context const context{
      .runway_heading{static_cast<tfx::math::angle>(heading)},
      .seconds{read_double(input)},
      .left_main_wheel_down{read_double(input)},
      .carrier{carrier != 0},
    };
    tfx::flight::aerodynamic_basis basis;
    bool const stopped{tfx::flight::advance_automatic_approach(state,motion,basis,speed,context)};
    std::vector<uint64_t> values;
    auto const number{[&](double const value) { values.push_back(std::bit_cast<uint64_t>(value)); }};
    for(auto const *vector : {&state.destination_metres,&state.player_metres}) for(auto const value : *vector) values.push_back(static_cast<uint32_t>(value));
    number(state.distance_km); number(state.pitch_degrees); number(speed);
    for(auto const value : motion.pose.metres_ned) number(value);
    for(auto const value : motion.rotation_remainder) number(value);
    for(auto const *vector : {&basis.forward,&basis.right,&basis.down}) for(auto const value : *vector) number(value);
    for(unsigned int row{0}; row != 3; ++row) for(auto const *vector : {&motion.pose.frame.forward,&motion.pose.frame.right,&motion.pose.frame.down}) values.push_back(static_cast<uint32_t>((*vector)[row]));
    values.push_back(state.flaring); values.push_back(stopped);
    unsigned int field{0};
    for(auto const value : values) {
      if(argc == 2) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native automatic approach differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
      } else std::cout << value << ' ';
      ++field;
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no automatic approach cases"};
  if(argc == 2) std::cout << cases << " native automatic approach fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
