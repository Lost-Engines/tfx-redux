#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_response.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic response fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int warp,landed,autopilot,inhibited,realistic,pressure,cases{0};
  while(input >> warp >> landed >> autopilot >> inhibited >> realistic >> pressure) {
    tfx::flight::aerodynamic_response_state state;
    tfx::flight::aerodynamic_response_context context{
      .time_warp{warp != 0},
      .landed{landed != 0},
      .autopilot{autopilot != 0},
      .rudder_inhibited{inhibited != 0},
      .realistic{realistic != 0},
      .stick_pressure{pressure != 0},
    };
    auto &motion{state.motion};
    for(auto *vector : {&motion.velocity_ned,&motion.acceleration_ned,&motion.ground_velocity_body,&motion.air_velocity_body,&motion.angular_rate}) for(auto &value : *vector) value = read_double(input);
    for(auto &value : motion.angular_acceleration) value = read_double(input);
    state.sideslip_radians = read_double(input);
    motion.yaw_step_radians = read_double(input);
    state.previous_pitch_rate = read_double(input);
    motion.pose.metres_ned.z = read_double(input);
    uint32_t g_bits; input >> g_bits;
    state.g_load = std::bit_cast<float>(g_bits);
    for(auto *vector : {&context.basis.forward,&context.basis.right,&context.basis.down}) for(auto &value : *vector) value = read_double(input);
    for(auto *value : {&context.inertia.mass_kg,&context.inertia.roll_moment,&context.inertia.pitch_moment,&context.inertia.fuel_kg}) *value = read_double(input);
    auto &p{context.parameters};
    for(auto *value : {&p.wing_area,&p.wing_span,&p.mean_chord,&p.maximum_aileron_radians,&p.rudder_effectiveness,&p.roll_damping,&p.pitch_damping,&p.pitch_stability,&p.zero_pitch_moment,&p.roll_thrust_arm,&p.pitch_thrust_arm,&p.maximum_elevator_angle}) *value = read_double(input);
    for(auto *value : {&context.seconds,&context.metres_per_second,&context.density,&context.stall_metres_per_second,&context.rudder,&context.aileron,&context.elevator,&context.roll_control_gradient,&context.pitch_control_gradient,&context.engine_thrust,&context.thrust_moment_scale,&context.angle_of_attack,&context.pitch_rate_bias,&context.ground_vertical_force}) *value = read_double(input);
    tfx::flight::update_aerodynamic_response(state,context);
    std::vector<uint64_t> values;
    for(auto const *vector : {&motion.velocity_ned,&motion.acceleration_ned,&motion.ground_velocity_body,&motion.air_velocity_body,&motion.angular_rate}) for(auto const value : *vector) values.push_back(std::bit_cast<uint64_t>(value));
    for(auto const value : motion.angular_acceleration) values.push_back(std::bit_cast<uint64_t>(value));
    for(auto const value : {state.sideslip_radians,motion.yaw_step_radians,state.previous_pitch_rate}) values.push_back(std::bit_cast<uint64_t>(value));
    values.push_back(std::bit_cast<uint32_t>(state.g_load));
    values.push_back(state.low_speed_ground_contact);
    if(argc == 2) {
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic response differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic response cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic response fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
