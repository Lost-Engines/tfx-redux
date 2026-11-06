#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/aerodynamic_approach_reference.h"

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
  if(argc == 2 && !fixture) throw std::runtime_error{"cannot open aerodynamic approach fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int craft,condition,cases{0};
  while(input >> craft >> condition) {
    tfx::flight::aerodynamic_approach_reference state;
    std::array<double *,5> const fields{&state.flap_fraction,&state.lift_coefficient,&state.angle_of_attack,&state.flight_path_degrees,&state.ground_speed};
    for(auto *value : fields) *value = read_double(input);
    tfx::flight::aerodynamic_motion motion;
    for(auto &value : motion.velocity_ned) value = read_double(input);
    tfx::flight::aerodynamic_airdata data;
    data.atmosphere.density = read_double(input);
    data.mass_kg = read_double(input);
    double const speed{read_double(input)};
    tfx::flight::aerodynamic_surface_parameters geometry;
    for(auto *value : {&geometry.wing_area,&geometry.flap_lift_gradient,&geometry.maximum_flap_angle,&geometry.zero_lift}) *value = read_double(input);
    tfx::flight::aerodynamic_coefficients coefficients;
    coefficients.lift_slope = read_double(input);
    tfx::flight::update_aerodynamic_approach_reference(state,{
      .data{data},
      .motion{motion},
      .coefficients{coefficients},
      .geometry{geometry},
      .aircraft{static_cast<tfx::flight::player_aircraft>(craft)},
      .low_speed_reference{speed},
      .flaps_condition{static_cast<tfx::flight::system_condition>(condition)},
    });
    unsigned int field{0};
    for(auto const *value : fields) {
      auto const bits{std::bit_cast<uint64_t>(*value)};
      if(argc == 2) {
        if(!tfx::test::fixture_matches(input,tfx::test::numeric_fixture_value{*value})) throw std::runtime_error{"native aerodynamic approach differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
      } else std::cout << bits << ' ';
      ++field;
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic approach cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic approach fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
