#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/aerodynamic_commands.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic command fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int pressure,cases{0};
  while(input >> pressure) {
    tfx::flight::aerodynamic_commands state;
    for(auto *value : {&state.roll_rate,&state.angle_of_attack,&state.body_lift_force}) *value = read_double(input);
    tfx::flight::aerodynamic_manual_state manual;
    for(auto *value : {&manual.controls.elevator,&manual.controls.aileron,&manual.thrust_vector_fraction,&manual.pitch_rate_bias}) *value = read_double(input);
    tfx::flight::aerodynamic_powerplant powerplant;
    powerplant.thrust = read_double(input);
    tfx::flight::aerodynamic_airdata data;
    for(auto *value : {&data.angle_of_attack,&data.metres_per_second,&data.atmosphere.density,&data.mass_kg}) *value = read_double(input);
    tfx::flight::aerodynamic_coefficients coefficients;
    for(auto *value : {&coefficients.elevator_moment,&coefficients.pitch_damping,&coefficients.pitch_stability,&coefficients.aileron_effectiveness,&coefficients.roll_damping}) *value = read_double(input);
    tfx::flight::aerodynamic_surface_parameters geometry;
    for(auto *value : {&geometry.wing_area,&geometry.mean_chord,&geometry.wing_span,&geometry.maximum_elevator_angle,&geometry.maximum_aileron_angle,&geometry.zero_pitch_moment}) *value = read_double(input);
    tfx::flight::aerodynamic_command_context context{
      .data{data},
      .coefficients{coefficients},
      .geometry{geometry},
      .engine_roll_arm{read_double(input)},
      .engine_pitch_arm{read_double(input)},
      .body_forward_force{read_double(input)},
      .body_down_force{read_double(input)},
      .seconds{read_double(input)},
      .stick_pressure{pressure != 0},
    };
    tfx::flight::update_aerodynamic_commands(state,manual,powerplant,context);
    // Lift subtracts sin(alpha)*forward and cos(alpha)*down. A one-ulp libm
    // difference can dominate the small residual when these forces cancel.
    // Four epsilons of the input force magnitudes cover trig and product
    // rounding plus subtraction. Other fields keep the usual result bound.
    double const lift_roundoff{context.data.metres_per_second > 1
      ? 4 * std::numeric_limits<double>::epsilon()
          * (std::abs(context.body_forward_force) + std::abs(context.body_down_force))
      : 0};
    using tfx::test::numeric_fixture_value;
    for(auto const actual : {numeric_fixture_value{state.roll_rate},
                            numeric_fixture_value{state.angle_of_attack},
                            numeric_fixture_value::with_absolute_roundoff(state.body_lift_force,lift_roundoff)}) {
      if(argc == 2) {
        if(!tfx::test::fixture_matches(input,actual)) throw std::runtime_error{"native aerodynamic command differs"};
      } else std::cout << actual << ' ';
    }
    if(argc == 1) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic command cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic command fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
