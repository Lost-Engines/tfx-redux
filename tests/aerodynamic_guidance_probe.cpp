#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <string_view>
#include "flight/aerodynamic_guidance.h"

namespace {
double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  bool const control_mode{argc >= 2 && std::string_view{argv[1]} == "--controls"};
  char const *fixture_path{nullptr};
  if(control_mode && argc == 3) fixture_path = argv[2];
  else if(!control_mode && argc == 2) fixture_path = argv[1];
  if(argc > (control_mode ? 3 : 2)) throw std::runtime_error{"expected [--controls] [FILE]"};
  std::ifstream fixture;
  if(fixture_path) fixture.open(fixture_path);
  if(fixture_path && !fixture) throw std::runtime_error{"cannot open aerodynamic guidance fixtures"};
  auto &input{fixture_path ? fixture : std::cin};
  unsigned int craft,military,followed,warp,mode,enabled,heading,brake,afterburner,control,display,throttle,cases{0}; int track;
  while(input >> craft >> military >> followed >> warp >> mode >> enabled >> heading >> track >> brake >> afterburner >> control >> display >> throttle) {
    tfx::flight::aerodynamic_guidance state;
    std::array<double *,14> const fields{&state.heading_error,&state.ground_speed,&state.climb_speed_limit,&state.requested_vertical_speed,&state.desired_flight_path,&state.actual_flight_path,&state.maximum_bank,&state.desired_bank,&state.roll_angle,&state.roll_error,&state.roll_control_rate,&state.commanded_roll_rate,&state.saved_roll_rate,&state.yaw_demand};
    for(auto *value : fields) *value = read_double(input);
    tfx::flight::aerodynamic_manual_state manual;
    manual.controls.autopilot.mode = static_cast<uint16_t>(mode);
    manual.controls.autopilot.enabled = enabled != 0;
    manual.controls.autopilot.heading = static_cast<tfx::math::angle>(heading);
    manual.air_brake_condition = static_cast<tfx::flight::system_condition>(brake);
    for(auto *value : {&manual.pitch_rate_bias,&manual.controls.autopilot.down_metres,&manual.controls.autopilot.metres_per_second,&manual.air_brake_fraction}) *value = read_double(input);
    tfx::flight::aerodynamic_powerplant powerplant;
    powerplant.throttle = std::bit_cast<float>(throttle);
    powerplant.afterburner = static_cast<uint8_t>(afterburner);
    powerplant.afterburner_control = static_cast<uint8_t>(control);
    powerplant.afterburner_display = static_cast<uint8_t>(display);
    for(auto *value : {&powerplant.engine_factor.x,&powerplant.engine_factor.y,&powerplant.thrust,&powerplant.throttle_target,&powerplant.fuel}) *value = read_double(input);
    tfx::flight::aerodynamic_motion motion;
    for(auto *vector : {&motion.velocity_ned,&motion.acceleration_ned,&motion.angular_rate,&motion.pose.metres_ned}) for(auto &value : *vector) value = read_double(input);
    for(auto &value : motion.rotation_remainder) value = read_double(input);
    tfx::flight::aerodynamic_surface_parameters geometry;
    for(auto *value : {&geometry.wing_area,&geometry.wing_span,&geometry.mean_chord,&geometry.maximum_aileron_angle,&geometry.maximum_elevator_angle,&geometry.air_brake_drag}) *value = read_double(input);
    tfx::flight::aerodynamic_airdata data;
    data.track_heading = track;
    for(auto *value : {&data.atmosphere.density,&data.metres_per_second,&data.mass_kg}) *value = read_double(input);
    tfx::flight::aerodynamic_coefficients coefficients;
    for(auto *value : {&coefficients.roll_damping,&coefficients.pitch_damping,&coefficients.aileron_effectiveness}) *value = read_double(input);
    tfx::flight::aerodynamic_surface_state surfaces;
    surfaces.positive_lift_limit = read_double(input);
    manual.positive_g_lift_limit = read_double(input);
    tfx::flight::aerodynamic_powerplant_parameters engine;
    engine.aircraft = static_cast<tfx::flight::player_aircraft>(craft);
    engine.afterburner_stages = craft == 2 ? 0 : 4;
    engine.dry_thrust = read_double(input);
    engine.afterburner_thrust = read_double(input);
    double const roll_moment{read_double(input)}, pitch_moment{read_double(input)}, drag{read_double(input)}, seconds{read_double(input)}, forward_force{read_double(input)};
    for(auto *vector : {&data.basis.forward,&data.basis.right,&data.basis.down}) for(auto &value : *vector) value = read_double(input);
    for(unsigned int row{0}; row != 3; ++row) input >> motion.pose.frame.forward[row] >> motion.pose.frame.right[row] >> motion.pose.frame.down[row];
    std::array<double *,7> const control_fields{&manual.controls.elevator,&manual.controls.aileron,&manual.controls.rudder,&manual.controls.keyboard_elevator,&manual.controls.keyboard_aileron,&manual.controls.keyboard_rudder,&manual.elevator_trim};
    double rudder_effectiveness{.3};
    if(control_mode) {
      for(auto *value : {&coefficients.elevator_moment,&coefficients.pitch_stability,&geometry.zero_pitch_moment,&data.angle_of_attack,&engine.roll_thrust_arm,&engine.pitch_thrust_arm,&rudder_effectiveness,&manual.thrust_vector_fraction}) *value = read_double(input);
      for(auto *value : control_fields) *value = read_double(input);
    }
    tfx::flight::aerodynamic_manual_context const law{
      .data{data},
      .coefficients{coefficients},
      .surfaces{surfaces},
      .geometry{geometry},
      .aircraft{static_cast<tfx::flight::player_aircraft>(craft)},
      .body_forward_force{forward_force},
      .afterburner_stages{engine.afterburner_stages},
      .military{military != 0},
    };
    tfx::flight::aerodynamic_recovery_state recovery{
      .configuration{5},
      .saved_configuration{1},
      .saved_mode{0},
      .saved_enabled{false},
      .saved_air_brake{tfx::flight::system_condition::normal},
      .saved_throttle{.7},
      .saved_afterburner{0},
    };
    tfx::flight::aerodynamic_guidance_context const context{
      .law{law},
      .engine{engine},
      .seconds{seconds},
      .roll_moment{roll_moment},
      .pitch_moment{pitch_moment},
      .previous_drag{drag},
      .rudder_effectiveness{rudder_effectiveness},
      .followed_aircraft{followed != 0},
      .time_warp{warp != 0},
      .recovery{
        .afterburner_stages{engine.afterburner_stages},
        .available{true},
      },
    };
    tfx::flight::aerodynamic_guidance_events events;
    if(control_mode) tfx::flight::apply_aerodynamic_guidance(state,manual,powerplant,context);
    else events = tfx::flight::update_aerodynamic_guidance(state,manual,powerplant,motion,recovery,context);
    std::vector<uint64_t> values;
    auto const number{[&](double const value) { values.push_back(std::bit_cast<uint64_t>(value)); }};
    for(auto const *value : fields) number(*value);
    for(auto const value : {manual.pitch_rate_bias,manual.controls.autopilot.down_metres,manual.controls.autopilot.metres_per_second,manual.air_brake_fraction,powerplant.engine_factor.x,powerplant.engine_factor.y,powerplant.thrust,powerplant.throttle_target,powerplant.fuel}) number(value);
    for(auto const *vector : {&motion.velocity_ned,&motion.acceleration_ned,&motion.angular_rate,&motion.pose.metres_ned}) for(auto const value : *vector) number(value);
    for(auto const value : motion.rotation_remainder) number(value);
    for(auto const value : {static_cast<unsigned int>(recovery.configuration),static_cast<unsigned int>(manual.controls.autopilot.mode),static_cast<unsigned int>(manual.controls.autopilot.enabled),static_cast<unsigned int>(manual.air_brake_condition),static_cast<unsigned int>(powerplant.afterburner),static_cast<unsigned int>(powerplant.afterburner_control),static_cast<unsigned int>(powerplant.afterburner_display),std::bit_cast<uint32_t>(powerplant.throttle),static_cast<unsigned int>(recovery.model_reset_requested)}) values.push_back(value);
    for(unsigned int row{0}; row != 3; ++row) for(auto const *vector : {&motion.pose.frame.forward,&motion.pose.frame.right,&motion.pose.frame.down}) values.push_back(static_cast<uint32_t>((*vector)[row]));
    values.push_back(static_cast<unsigned int>(events.recovery));
    values.push_back(events.afterburner.size());
    for(auto const change : events.afterburner) values.push_back(static_cast<unsigned int>(change));
    if(control_mode) for(auto const *value : control_fields) number(*value);
    if(fixture_path) {
      unsigned int field{0};
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic guidance differs at case " + std::to_string(cases) + " field " + std::to_string(field)};
        ++field;
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic guidance cases"};
  if(fixture_path) std::cout << cases << " native aerodynamic guidance fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
