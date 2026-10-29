#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_motion.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic motion fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int simplified,warp,landed,crashing,inhibited,cases{0};
  while(input >> simplified >> warp >> landed >> crashing >> inhibited) {
    tfx::flight::aerodynamic_motion state;
    input >> state.collision >> state.integer_world_y;
    for(auto *vector : {&state.pose.metres_ned,&state.velocity_ned,&state.acceleration_ned,&state.angular_rate}) for(auto &value : *vector) value = read_double(input);
    for(auto *vector : {&state.angular_acceleration,&state.rotation_remainder}) for(auto &value : *vector) value = read_double(input);
    state.yaw_step_radians = read_double(input);
    for(unsigned int row{0}; row != 3; ++row) input >> state.pose.frame.forward[row] >> state.pose.frame.right[row] >> state.pose.frame.down[row];
    tfx::flight::aerodynamic_motion_context const context{
      .seconds{read_double(input)},
      .time_warp{warp != 0},
      .landed{landed != 0},
      .crashing{crashing != 0},
      .simplified_rotation{simplified != 0},
      .roll_rate_inhibited{inhibited != 0},
      .commanded_roll_rate{read_double(input)},
      .commanded_angle_of_attack{read_double(input)},
      .angle_of_attack{read_double(input)},
    };
    tfx::flight::advance_aerodynamic_motion(state,context);
    std::vector<uint64_t> values;
    for(auto const *vector : {&state.pose.metres_ned,&state.velocity_ned,&state.acceleration_ned,&state.angular_rate}) for(auto const value : *vector) values.push_back(std::bit_cast<uint64_t>(value));
    for(auto const *vector : {&state.angular_acceleration,&state.rotation_remainder}) for(auto const value : *vector) values.push_back(std::bit_cast<uint64_t>(value));
    for(unsigned int row{0}; row != 3; ++row) for(auto const value : {state.pose.frame.forward[row],state.pose.frame.right[row],state.pose.frame.down[row]}) values.push_back(static_cast<uint32_t>(value));
    values.push_back(static_cast<uint16_t>(state.collision));
    values.push_back(static_cast<uint32_t>(state.integer_world_y));
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic motion differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic motion cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic motion fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
