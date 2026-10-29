#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/aerodynamic_navigation.h"

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
  if(argc == 2 && !fixture) throw std::runtime_error{"cannot open aerodynamic navigation fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int mode,heading,pitch,followed,count,cases{0}; int index;
  while(input >> mode >> heading >> index >> pitch >> followed >> count) {
    tfx::flight::aerodynamic_navigation state;
    state.route_index = static_cast<int16_t>(index);
    state.target_pitch = static_cast<tfx::math::angle>(pitch);
    for(auto *vector : {&state.target_position,&state.player_position}) for(auto &value : *vector) input >> value;
    tfx::flight::aerodynamic_follow_target target;
    for(auto &value : target.position_q8) input >> value;
    input >> target.speed_q16;
    tfx::flight::aerodynamic_guidance guidance;
    tfx::flight::aerodynamic_autopilot_targets targets;
    targets.mode = static_cast<uint16_t>(mode);
    targets.heading = static_cast<tfx::math::angle>(heading);
    for(auto *value : {&state.route_distance,&state.followed_speed_radians,&guidance.desired_flight_path,&guidance.saved_roll_rate,&guidance.ground_speed,&targets.down_metres}) *value = read_double(input);
    tfx::flight::aerodynamic_motion motion;
    for(auto &value : motion.pose.metres_ned) value = read_double(input);
    motion.angular_rate.x = read_double(input);
    std::vector<vec2<uint8_t>> cells;
    for(unsigned int cell{0}; cell != count; ++cell) {
      unsigned int x,z; input >> x >> z;
      cells.emplace_back(static_cast<uint8_t>(x),static_cast<uint8_t>(z));
    }
    tfx::flight::aerodynamic_navigation_context const context{
      .route_cells{cells},
      .followed_aircraft{followed ? std::optional<tfx::flight::aerodynamic_follow_target>{target} : std::nullopt},
    };
    tfx::flight::update_aerodynamic_navigation(state,guidance,targets,motion,context);
    std::vector<uint64_t> values{static_cast<uint16_t>(state.route_index),targets.heading,state.target_pitch};
    for(auto const *vector : {&state.target_position,&state.player_position}) for(auto const value : *vector) values.push_back(static_cast<uint32_t>(value));
    for(auto const value : {state.route_distance,state.followed_speed_radians,guidance.desired_flight_path,guidance.saved_roll_rate,guidance.ground_speed,targets.down_metres}) values.push_back(std::bit_cast<uint64_t>(value));
    if(argc == 2) {
      unsigned int field{0};
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic navigation differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
        ++field;
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic navigation cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic navigation fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
