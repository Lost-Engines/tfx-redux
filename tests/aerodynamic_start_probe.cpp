#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_start.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic start fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int craft,landed,cold,terrain,steering,occupied,weapon,cases{0};
  int ground_height,heading,fixed_speed;
  while(input >> craft >> landed >> cold >> terrain >> steering >> occupied >> weapon >> ground_height >> heading >> fixed_speed) {
    tfx::flight::aerodynamic_start_context context{
      .fixed_speed{fixed_speed},
      .heading{static_cast<tfx::math::angle>(heading)},
      .ground_elevation_steps{static_cast<int16_t>(ground_height)},
      .landed{landed != 0},
      .cold_start{cold != 0},
      .terrain_present{terrain != 0},
    };
    for(auto &value : context.position) {
      uint32_t bits; input >> bits;
      value = std::bit_cast<float>(bits);
    }
    for(unsigned int row{0}; row != 3; ++row) input >> context.orientation.right[row] >> context.orientation.down[row] >> context.orientation.forward[row];
    tfx::flight::aerodynamic_start_state state;
    uint32_t g; input >> g;
    state.g_load = std::bit_cast<float>(g);
    state.ground_steering = steering != 0;
    for(auto &value : state.motion.angular_rate) value = read_double(input);
    for(auto &value : state.motion.acceleration_ned) value = read_double(input);
    for(auto &value : state.motion.angular_acceleration) value = read_double(input);
    for(auto &value : state.engine_factor) value = read_double(input);
    state.fuel_kg = read_double(input);
    state.stores.mass_kg = read_double(input);
    tfx::flight::aerodynamic_airframe frame;
    frame.stores_and_mounts.stores.aircraft = static_cast<tfx::flight::player_aircraft>(craft);
    for(auto *value : {&frame.empty_mass_kg,&frame.viewpoint.forward,&frame.viewpoint.upward,&frame.ground.contact.spring_stiffness,&frame.ground.offsets[1].z,&frame.stores_and_mounts.stores.fuel_capacity,&frame.stores_and_mounts.stores.moment_a,&frame.stores_and_mounts.stores.moment_b,&frame.carrier_setup.maximum_sink_speed,&frame.carrier_setup.wing_area,&frame.carrier_setup.air_brake_drag,&frame.ground.contact.maximum_sink_speed,&frame.surfaces.wing_area,&frame.surfaces.air_brake_drag}) *value = read_double(input);
    frame.coefficient_base.wing_area = frame.surfaces.wing_area;
    tfx::flight::loadout stores;
    auto &store{stores[0]};
    store.occupied = occupied != 0;
    for(auto &value : store.position) value = read_double(input);
    int mass; uint32_t drag; unsigned int tank;
    input >> mass >> drag >> tank;
    store.properties = {
      .mass_kg{static_cast<int16_t>(mass)},
      .drag{std::bit_cast<float>(drag)},
      .fuel_tank{tank != 0},
    };
    tfx::flight::initialise_aerodynamic_motion(state,frame,stores,context);
    std::vector<uint64_t> values;
    auto const number{[&](double const value){ values.push_back(std::bit_cast<uint64_t>(value)); }};
    for(auto const *vector : {&state.motion.pose.metres_ned,&state.motion.velocity_ned,&state.motion.acceleration_ned,&state.motion.ground_velocity_body,&state.motion.angular_rate}) for(auto const value : *vector) number(value);
    for(auto const value : state.motion.angular_acceleration) number(value);
    for(auto const value : state.engine_factor) number(value);
    for(auto const value : {state.fuel_kg,state.mass_kg,state.seconds,state.stores.mass_kg,state.stores.drag,state.stores.moment_a,state.stores.moment_b,state.stores.total_moment_a,state.stores.total_moment_b,state.stores.fuel_capacity,frame.ground.contact.maximum_sink_speed,frame.surfaces.wing_area,frame.surfaces.air_brake_drag}) number(value);
    if(frame.surfaces.wing_area != frame.coefficient_base.wing_area) throw std::runtime_error{"carrier setup left inconsistent wing areas"};
    for(auto const value : {std::bit_cast<uint32_t>(state.g_load),static_cast<uint32_t>(state.track_heading),static_cast<uint32_t>(state.ground_steering),static_cast<uint32_t>(state.stores.fuel_tank)}) values.push_back(value);
    for(unsigned int row{0}; row != 3; ++row) for(auto const *vector : {&state.motion.pose.frame.forward,&state.motion.pose.frame.right,&state.motion.pose.frame.down}) values.push_back(static_cast<uint32_t>((*vector)[row]));
    if(argc == 2) {
      unsigned int field{0};
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic start differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
        ++field;
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic start cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic start fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
