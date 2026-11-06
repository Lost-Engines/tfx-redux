#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>
#include "flight/aerodynamic_wind.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc == 3 && std::string_view{argv[1]} == "--parameters") {
    for(auto const &p : tfx::flight::load_aerodynamic_wind_parameters(tfx::resources::read_file(argv[2]))) {
      for(auto const value : {p.direction_wobble_scale,p.surface_speed,p.upper_speed,p.upper_direction_degrees,p.legacy_gust_period,p.legacy_gust_range}) std::cout << std::bit_cast<uint64_t>(value) << ' ';
      std::cout << '\n';
    }
    return 0;
  }
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic wind fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int op,weather,terrain,boost,low,high,cases{0};
  while(input >> op >> weather >> terrain >> boost >> low >> high) {
    tfx::flight::aerodynamic_wind state;
    std::array<double *,19> const fields{&state.simulation_seconds,&state.smooth_speed,&state.target_speed,&state.direction,&state.vertical_angle,&state.renewal_after,&state.direction_wobble_scale,&state.direction_phase_frequency,&state.direction_phase_offset,&state.direction_envelope_frequency,&state.vertical_phase_frequency,&state.vertical_phase_offset,&state.vertical_scale,&state.upper_wind.x,&state.upper_wind.y,&state.upper_mix,&state.legacy_gust_period,&state.legacy_gust_target,&state.legacy_gust_smoothing};
    for(auto *value : fields) *value = read_double(input);
    for(auto &value : state.velocity_ned) {
      uint32_t bits; input >> bits;
      value = std::bit_cast<float>(bits);
    }
    tfx::flight::aerodynamic_wind_parameters parameters;
    for(auto *value : {&parameters.direction_wobble_scale,&parameters.surface_speed,&parameters.upper_speed,&parameters.upper_direction_degrees,&parameters.legacy_gust_period,&parameters.legacy_gust_range}) *value = read_double(input);
    tfx::flight::aerodynamic_wind_context const context{
      .down_metres{read_double(input)},
      .terrain_wind_boost{terrain != 0},
      .weather_wind_boost{boost != 0},
    };
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    if(op) tfx::flight::update_aerodynamic_wind(state,parameters,random,context);
    else tfx::flight::initialise_aerodynamic_wind(state,parameters,random,context,weather == tfx::flight::aerodynamic_weather_profiles - 1);
    std::vector<tfx::test::numeric_fixture_value> values;
    for(auto const *value : fields) values.emplace_back(*value);
    for(auto const value : state.velocity_ned) values.emplace_back(value);
    values.push_back(random.low);
    values.push_back(random.high);
    if(argc == 2) {
      unsigned int field{0};
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic wind differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
        ++field;
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic wind cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic wind fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
