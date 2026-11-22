#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#include "flight/aerodynamic_realistic_ground.h"

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
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open Realistic aerodynamic ground fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int kind;
  unsigned int terrain;
  unsigned int damage_enabled;
  unsigned int low_ground;
  unsigned int wet;
  unsigned int rough;
  unsigned int wheel_brakes;
  unsigned int gear;
  unsigned int brakes;
  unsigned int seed_low;
  unsigned int seed_high;
  unsigned int cases{0};
  int ground_height;
  int downward;
  int collision;
  while(input >> kind >> terrain >> damage_enabled >> low_ground >> wet >> rough >> wheel_brakes >> gear >> brakes >> seed_low >> seed_high >> ground_height >> downward >> collision) {
    tfx::flight::aerodynamic_realistic_ground_state realistic;
    auto &ground{realistic.ground};
    auto &state{ground.contact};
    tfx::flight::aerodynamic_motion motion;
    motion.collision = static_cast<int16_t>(collision);
    tfx::flight::damage_state damage;
    damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = static_cast<tfx::flight::system_condition>(gear);
    damage.systems[std::to_underlying(tfx::flight::aircraft_system::wheel_brakes)] = static_cast<tfx::flight::system_condition>(brakes);
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(seed_low)},
      .high{static_cast<uint16_t>(seed_high)},
    };
    std::vector<double *> fields;
    for(auto *vector : {&state.offset_body, &state.point_ned, &state.velocity_body, &state.velocity_ned, &state.working_force, &state.force, &state.moment}) {
      for(auto &value : *vector) fields.push_back(&value);
    }
    for(auto *value : {&state.stiffness, &state.damping, &state.penetration, &state.down_velocity, &state.rolling_coefficient, &state.braking_coefficient, &state.resistance_force, &state.unloaded_clearance, &motion.angular_rate.z, &damage.accumulated}) fields.push_back(value);
    for(auto *value : fields) *value = read_double(input);
    tfx::flight::aerodynamic_ground_parameters geometry;
    auto &parameters{geometry.contact};
    for(auto *value : {&parameters.spring_stiffness, &parameters.damping, &parameters.maximum_sink_speed, &parameters.rolling_warning_speed, &parameters.rolling_failure_speed, &parameters.brake_warning_speed, &parameters.brake_failure_speed, &parameters.braking_coefficient, &parameters.wet_rolling_coefficient, &parameters.wet_braking_factor, &parameters.empty_mass_kg, &parameters.left_main_wheel_right, &parameters.unloaded_clearance}) *value = read_double(input);
    for(auto *vector : {&motion.pose.metres_ned, &motion.ground_velocity_body, &motion.velocity_ned}) {
      for(auto &value : *vector) value = read_double(input);
    }
    motion.angular_rate.x = read_double(input);
    motion.angular_rate.y = read_double(input);
    tfx::flight::aerodynamic_contact_context context{
      .kind{static_cast<tfx::flight::aerodynamic_contact_kind>(kind)},
      .ground_elevation_steps{static_cast<int16_t>(ground_height)},
      .body_downward_q15{static_cast<int16_t>(downward)},
      .total_mass_kg{read_double(input)},
      .terrain_present{terrain != 0},
      .gear_damage_enabled{damage_enabled != 0},
      .low_speed_ground_contact{low_ground != 0},
      .precipitation{wet != 0},
      .rough_surface{rough != 0},
      .wheel_brakes{wheel_brakes != 0},
    };
    for(auto *vector : {&context.basis.forward, &context.basis.right, &context.basis.down}) {
      for(auto &value : *vector) value = read_double(input);
    }
    tfx::flight::aerodynamic_ground_context ground_context{
      .contact{context},
    };
    std::vector<double *> ground_fields;
    for(auto *vector : {&ground.hook_offset_ned, &ground.hook_anchor_ned, &ground.hook_displacement, &ground.hook_force_ned, &ground.hook_force_body, &ground.hook_moment}) {
      for(auto &value : *vector) ground_fields.push_back(&value);
    }
    for(auto *value : {&ground.cable_length, &ground.cable_speed, &ground.rollout_distance, &state.ground_up_metres}) ground_fields.push_back(value);
    for(auto *value : ground_fields) *value = read_double(input);
    unsigned int phase;
    unsigned int steering;
    unsigned int landed;
    unsigned int last_kind;
    input >> phase >> steering >> landed >> last_kind;
    ground.arrestor = static_cast<tfx::flight::aerodynamic_arrestor_phase>(phase);
    ground.ground_steering = steering != 0;
    ground.landed = landed != 0;
    ground.last_contact_kind = static_cast<tfx::flight::aerodynamic_contact_kind>(last_kind);
    for(auto &offset : geometry.offsets) {
      for(auto &value : offset) value = read_double(input);
    }
    for(auto &value : geometry.hook_offset) value = read_double(input);
    for(auto *value : {&geometry.maximum_gear_speed, &geometry.arrestor_stiffness, &geometry.arrestor_damping, &ground_context.gear_fraction, &ground_context.metres_per_second, &ground_context.seconds, &ground_context.engine_factor.x, &ground_context.engine_factor.y}) *value = read_double(input);
    for(auto &value : ground_context.previous_ned_force) value = read_double(input);
    uint32_t throttle;
    unsigned int advanced;
    unsigned int carrier;
    unsigned int hook;
    input >> throttle >> advanced >> carrier >> hook;
    ground_context.throttle = std::bit_cast<float>(throttle);
    ground_context.non_arcade_model = advanced != 0;
    ground_context.carrier_surface = carrier != 0;
    ground_context.arrestor_hook_down = hook != 0;
    tfx::flight::aerodynamic_control_state controls;
    std::vector<double *> realistic_fields;
    for(auto &value : realistic.wheel_penetration) realistic_fields.push_back(&value);
    for(auto *value : {&realistic.roll_angle, &realistic.pitch_angle, &realistic.commanded_roll_rate, &realistic.commanded_angle_of_attack, &controls.keyboard_elevator}) realistic_fields.push_back(value);
    for(auto *value : realistic_fields) *value = read_double(input);
    tfx::flight::aerodynamic_realistic_ground_context realistic_context{
      .ground{ground_context},
      .stall_metres_per_second{read_double(input)},
      .angle_of_attack{read_double(input)},
      .lift{read_double(input)},
    };
    motion.air_velocity_body.x = read_double(input);
    auto const events{tfx::flight::update_aerodynamic_realistic_ground(realistic, motion, controls, damage, random, geometry, realistic_context)};
    std::vector<uint64_t> values;
    for(auto const *value : fields) values.push_back(std::bit_cast<uint64_t>(*value));
    for(auto const *value : ground_fields) values.push_back(std::bit_cast<uint64_t>(*value));
    for(auto const value : {static_cast<unsigned int>(damage.condition(tfx::flight::aircraft_system::landing_gear)), static_cast<unsigned int>(damage.condition(tfx::flight::aircraft_system::wheel_brakes)), static_cast<unsigned int>(static_cast<uint16_t>(motion.collision)), static_cast<unsigned int>(random.low), static_cast<unsigned int>(random.high), events.gear_impacts, static_cast<unsigned int>(events.cockpit_bump), static_cast<unsigned int>(ground.arrestor), static_cast<unsigned int>(ground.ground_steering), static_cast<unsigned int>(ground.landed), static_cast<unsigned int>(ground.last_contact_kind)}) values.push_back(value);
    for(auto const *value : realistic_fields) values.push_back(std::bit_cast<uint64_t>(*value));
    values.push_back(std::bit_cast<uint64_t>(motion.pose.metres_ned.z));
    values.push_back(std::bit_cast<uint64_t>(motion.velocity_ned.z));
    if(argc == 2) {
      unsigned int field{0};
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native Realistic aerodynamic ground differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
        ++field;
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no Realistic aerodynamic ground cases"};
  if(argc == 2) std::cout << cases << " native Realistic aerodynamic ground fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
