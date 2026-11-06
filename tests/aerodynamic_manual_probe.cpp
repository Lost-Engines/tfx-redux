#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_manual.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open manual aerodynamic fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int aircraft,source,military,pressure,flaps,afterburner,control,display,brake,cases{0};
  while(input >> aircraft >> source >> military >> pressure >> flaps >> afterburner >> control >> display >> brake) {
    tfx::flight::aerodynamic_manual_state state;
    state.air_brake_condition = static_cast<tfx::flight::system_condition>(brake);
    tfx::flight::aerodynamic_powerplant powerplant;
    powerplant.afterburner = static_cast<uint8_t>(afterburner);
    powerplant.afterburner_control = static_cast<uint8_t>(control);
    powerplant.afterburner_display = static_cast<uint8_t>(display);
    uint32_t throttle; input >> throttle;
    powerplant.throttle = std::bit_cast<float>(throttle);
    auto &controls{state.controls};
    std::array<double *,16> const state_fields{&controls.elevator,&controls.aileron,&controls.rudder,&controls.keyboard_elevator,&controls.keyboard_aileron,&controls.keyboard_rudder,&controls.minimum_elevator,&controls.maximum_elevator,&controls.aileron_limit,&state.elevator_trim,&state.pitch_rate_bias,&state.flap_fraction,&state.positive_g_lift_limit,&state.negative_g_lift_limit,&state.thrust_vector_fraction,&state.air_brake_fraction};
    for(auto *value : state_fields) *value = read_double(input);
    for(auto *value : {&powerplant.engine_factor.x,&powerplant.engine_factor.y,&powerplant.nozzle_angles.x,&powerplant.nozzle_angles.y,&powerplant.thrust,&powerplant.throttle_target,&powerplant.fuel,&controls.autopilot.metres_per_second}) *value = read_double(input);
    tfx::flight::aerodynamic_airdata data;
    for(auto *vector : {&data.basis.forward,&data.basis.right,&data.basis.down}) for(auto &value : *vector) value = read_double(input);
    for(auto *value : {&data.angle_of_attack,&data.metres_per_second,&data.mach,&data.atmosphere.density,&data.atmosphere.sound_metres_per_second,&data.mass_kg,&data.simulation_seconds}) *value = read_double(input);
    tfx::flight::aerodynamic_coefficients coefficients;
    for(auto *value : {&coefficients.reference_drag,&coefficients.wave_drag,&coefficients.zero_lift_drag,&coefficients.lift_slope,&coefficients.pitch_stability,&coefficients.pitch_damping,&coefficients.elevator_moment,&coefficients.elevator_lift,&coefficients.roll_damping,&coefficients.roll_sideslip,&coefficients.aileron_effectiveness}) *value = read_double(input);
    tfx::flight::aerodynamic_surface_parameters geometry;
    for(auto *value : {&geometry.wing_area,&geometry.wing_span,&geometry.mean_chord,&geometry.flap_lift_gradient,&geometry.maximum_flap_angle,&geometry.zero_lift,&geometry.critical_angle,&geometry.zero_pitch_moment,&geometry.maximum_aileron_angle,&geometry.air_brake_drag,&geometry.maximum_elevator_angle}) *value = read_double(input);
    tfx::flight::aerodynamic_surface_state surfaces;
    for(auto *value : {&surfaces.positive_lift_limit,&surfaces.negative_lift_limit,&surfaces.flap_lift_offset}) *value = read_double(input);
    tfx::flight::aerodynamic_manual_context context{
      .data{data},
      .coefficients{coefficients},
      .surfaces{surfaces},
      .geometry{geometry},
      .aircraft{static_cast<tfx::flight::player_aircraft>(aircraft)},
      .source{static_cast<tfx::flight::control_source>(source)},
      .maximum_g{read_double(input)},
      .minimum_g{read_double(input)},
      .cockpit_upward_metres{read_double(input)},
      .engine_pitch_arm{read_double(input)},
      .body_down_force{read_double(input)},
      .body_forward_force{read_double(input)},
      .accumulated_damage{read_double(input)},
      .afterburner_stages{aircraft == 2 ? 0u : 4u},
      .military{military != 0},
      .stick_pressure{pressure != 0},
      .flaps_condition{static_cast<tfx::flight::system_condition>(flaps)},
    };
    auto const change{tfx::flight::update_aerodynamic_manual(state,powerplant,context)};
    std::vector<tfx::test::numeric_fixture_value> values;
    for(auto const *value : state_fields) values.emplace_back(*value);
    for(auto const value : {powerplant.engine_factor.x,powerplant.engine_factor.y,powerplant.nozzle_angles.x,powerplant.nozzle_angles.y,powerplant.thrust,powerplant.throttle_target,powerplant.fuel,controls.autopilot.metres_per_second}) values.emplace_back(value);
    values.emplace_back(powerplant.throttle);
    for(auto const value : {static_cast<unsigned int>(powerplant.afterburner),static_cast<unsigned int>(powerplant.afterburner_control),static_cast<unsigned int>(powerplant.afterburner_display),static_cast<unsigned int>(state.air_brake_condition),static_cast<unsigned int>(change)}) values.push_back(value);
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native manual aerodynamic control differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no manual aerodynamic cases"};
  if(argc == 2) std::cout << cases << " native manual aerodynamic fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
