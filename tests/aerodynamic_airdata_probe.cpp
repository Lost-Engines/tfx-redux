#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_airdata.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic air-data fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int realistic,cases{0};
  int16_t temperature;
  while(input >> realistic >> temperature) {
    tfx::flight::aerodynamic_motion motion;
    tfx::flight::aerodynamic_airdata state;
    tfx::flight::aerodynamic_airdata_context context{
      .surface_celsius{realistic ? std::optional<int16_t>{temperature} : std::nullopt},
    };
    for(unsigned int row{0}; row != 3; ++row) input >> motion.pose.frame.forward[row] >> motion.pose.frame.right[row] >> motion.pose.frame.down[row];
    for(auto &value : motion.velocity_ned) value = read_double(input);
    for(auto &value : context.wind_ned) { uint32_t bits; input >> bits; value = std::bit_cast<float>(bits); }
    motion.pose.metres_ned.z = read_double(input);
    for(auto *value : {&context.empty_mass_kg,&context.fuel_kg,&context.stores_kg,&context.seconds,&state.simulation_seconds,&state.angle_of_attack}) *value = read_double(input);
    input >> state.track_heading;
    for(auto *value : {&context.atmosphere.sea_level_density,&context.atmosphere.temperature_lapse_per_metre,&context.atmosphere.temperature_lapse_limit_metres}) *value = read_double(input);
    tfx::flight::update_aerodynamic_airdata(state,motion,context);
    std::vector<tfx::test::numeric_fixture_value> values;
    for(auto const *vector : {&state.basis.forward,&state.basis.right,&state.basis.down,&motion.ground_velocity_body,&motion.air_velocity_body}) for(auto const value : *vector) values.emplace_back(value);
    for(auto const value : {state.atmosphere.density,state.atmosphere.sea_level_kelvin,state.atmosphere.local_kelvin,state.atmosphere.sound_metres_per_second,state.flight_path_pitch_degrees,state.altitude_km,state.angle_of_attack,state.metres_per_second,state.mach,state.dynamic_pressure,state.mass_kg,state.simulation_seconds}) values.emplace_back(value);
    values.push_back(static_cast<uint32_t>(state.fixed_speed));
    values.push_back(static_cast<uint32_t>(state.track_heading));
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic air data differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic air-data cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic air-data fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
