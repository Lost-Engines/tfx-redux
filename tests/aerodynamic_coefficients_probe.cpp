#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/aerodynamic_coefficients.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic coefficient fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int aircraft,cases{0};
  while(input >> aircraft) {
    auto const mach{read_double(input)};
    tfx::flight::aerodynamic_coefficient_parameters p;
    for(auto *value : {&p.base_drag,&p.critical_mach,&p.full_wave_drag_mach,&p.wave_drag_increment,&p.wing_area,&p.additional_lifting_area,&p.pitch_stability_base,&p.pitch_stability_correction,&p.pitch_damping_mach_gradient,&p.pitch_damping_base,&p.elevator_moment_gradient,&p.elevator_lift_gradient,&p.roll_damping_gradient,&p.aileron_gradient}) *value = read_double(input);
    auto const c{tfx::flight::update_aerodynamic_coefficients(static_cast<tfx::flight::player_aircraft>(aircraft),mach,p)};
    for(auto const value : {c.reference_drag,c.wave_drag,c.zero_lift_drag,c.lift_slope,c.pitch_stability,c.pitch_damping,c.elevator_moment,c.elevator_lift,c.roll_damping,c.roll_sideslip,c.aileron_effectiveness}) {
      auto const actual{std::bit_cast<uint64_t>(value)};
      if(argc == 2) {
        if(!tfx::test::fixture_matches(input,tfx::test::numeric_fixture_value{value})) throw std::runtime_error{"native aerodynamic coefficients differ"};
      } else std::cout << actual << ' ';
    }
    if(argc == 1) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic coefficient cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic coefficient fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
