#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_surfaces.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic surface fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int aircraft,chute,moments,condition,cases{0};
  while(input >> aircraft >> chute >> moments >> condition) {
    tfx::flight::aerodynamic_surface_state state;
    tfx::flight::aerodynamic_surface_context context{
      .aircraft{static_cast<tfx::flight::player_aircraft>(aircraft)},
      .chute_deployed{chute != 0},
      .calculate_moments{moments != 0},
      .flaps_condition{static_cast<tfx::flight::system_condition>(condition)},
    };
    std::array<double *,14> const state_fields{&state.automatic_flap_fraction,&state.flap_lift_offset,&state.positive_lift_limit,&state.negative_lift_limit,&state.wing_lift_coefficient,&state.effective_angle,&state.lift_coefficient,&state.lift,&state.drag_coefficient,&state.drag,&state.pitch_rate_moment_term,&state.pitch_moment,&state.roll_rate_moment_term,&state.roll_moment};
    for(auto *value : state_fields) *value = read_double(input);
    for(auto *vector : {&state.force,&state.moment}) for(auto &value : *vector) value = read_double(input);
    auto &c{context.coefficients};
    for(auto *value : {&c.reference_drag,&c.wave_drag,&c.zero_lift_drag,&c.lift_slope,&c.pitch_stability,&c.pitch_damping,&c.elevator_moment,&c.elevator_lift,&c.roll_damping,&c.roll_sideslip,&c.aileron_effectiveness}) *value = read_double(input);
    tfx::flight::aerodynamic_surface_parameters p;
    for(auto *value : {&p.wing_area,&p.wing_span,&p.mean_chord,&p.flap_lift_gradient,&p.maximum_flap_angle,&p.flap_limit_gradient,&p.maximum_lift,&p.automatic_flap_gradient,&p.maximum_automatic_flap_angle,&p.zero_lift,&p.critical_angle,&p.induced_drag,&p.air_brake_drag,&p.gear_drag,&p.elevator_drag,&p.chute_area,&p.zero_pitch_moment,&p.chute_pitch_arm,&p.air_brake_moment_arm.x,&p.air_brake_moment_arm.y,&p.maximum_aileron_angle,&p.maximum_elevator_angle}) *value = read_double(input);
    for(auto *value : {&context.angle_of_attack,&context.metres_per_second,&context.density,&context.dynamic_pressure,&context.flap_fraction,&context.gear_fraction,&context.air_brake_fraction,&context.elevator,&context.aileron,&context.sideslip,&context.pitch_rate,&context.roll_rate}) *value = read_double(input);
    tfx::flight::update_aerodynamic_surfaces(state,p,context);
    std::vector<tfx::test::numeric_fixture_value> values;
    for(auto const *value : state_fields) values.emplace_back(*value);
    for(auto const *vector : {&state.force,&state.moment}) for(auto const value : *vector) values.emplace_back(value);
    values.push_back(state.stalled);
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic surfaces differ"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic surface cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic surface fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
