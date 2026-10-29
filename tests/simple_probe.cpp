#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>
#include <utility>
#include "flight/simple.h"

namespace {

std::vector<uint64_t> words(tfx::flight::simple_state const &state) {
  auto const &flight{state.flight};
  std::vector<uint64_t> values;
  auto const floating{[&](float const value) { values.push_back(std::bit_cast<uint32_t>(value)); }};
  auto const integer{[&](int32_t const value) { values.push_back(static_cast<uint32_t>(value)); }};
  auto const precise{[&](double const value) { values.push_back(std::bit_cast<uint64_t>(value)); }};
  for(auto const value : flight.position) floating(value);
  for(auto const value : flight.integer_position) integer(value);
  for(auto const *basis : {&state.ground.body,&state.ground.visible,&flight.orientation}) for(unsigned int row{0}; row != 3; ++row) {
    integer(basis->right[row]);integer(basis->down[row]);integer(basis->forward[row]);
  }
  for(auto const value : flight.rotation_remainder) floating(value);
  for(auto const value : {flight.speed_mach,flight.throttle,flight.minimum_speed,flight.maximum_speed,flight.target_speed,flight.speed_metres_per_second,flight.altitude_km,flight.movement_per_tick}) floating(value);
  precise(flight.unrounded_speed_mach);
  integer(flight.fixed_speed);
  for(auto const value : {flight.landed,flight.low_speed_ground_contact,flight.recovering,flight.gear_down,flight.brake_chute,flight.brake_chute_cut_away,flight.arrestor_hook,flight.arrested,flight.left_engine,flight.right_engine}) integer(value);
  integer(flight.collision);integer(std::to_underlying(state.gear.condition));integer(flight.afterburner);integer(flight.fuel_readout);integer(flight.normalisation_counter);
  precise(flight.fuel);precise(flight.fuel_flow);
  for(auto const value : flight.angles) integer(value);
  for(auto const value : state.controls.angular_rate) floating(value);
  floating(state.controls.negative_pitch_limit);floating(state.controls.positive_pitch_limit);floating(state.controls.roll_limit);
  floating(state.turn.turn_response);floating(state.turn.coordinated_turn_rate);floating(state.turn.metres_per_second);floating(state.displayed_g_load);floating(state.turn.g_load);
  integer(state.turn.bank_correction);integer(state.ground.stall_pitch);integer(state.ground.landing_angles.x);integer(state.ground.landing_angles.y);integer(state.ground.previous_landed);
  precise(state.ground.indicated_speed);precise(state.ground.low_speed_reference);
  floating(state.geometric_altitude_km);floating(state.gear.vertical_speed);floating(state.reported_speed);
  return values;
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  bool const disabled{argc >= 2 && (std::string_view{argv[1]} == "--disabled" || std::string_view{argv[1]} == "--check-disabled")};
  bool const check{argc == 3 && (std::string_view{argv[1]} == "--check" || disabled)};
  if(argc != 1 && !check && !(argc == 2 && disabled)) throw std::runtime_error{"expected --check FILE or Simple input trace"};
  std::ifstream fixture;
  if(check) fixture.open(argv[2]);
  if(check && !fixture) throw std::runtime_error{"cannot open Simple flight fixtures"};
  auto &input{check ? fixture : std::cin};
  unsigned int aircraft,frames,cases{0};
  while(input >> aircraft >> frames) {
    vec3<int32_t> position;vec3<tfx::math::angle> angles;
    float speed;unsigned int source;bool landed,present,rudder;int height,entry;
    input >> position.x >> position.y >> position.z >> angles.x >> angles.y >> angles.z >> speed >> source >> landed >> present >> height >> rudder >> entry;
    tfx::flight::simple_state state;
    state.flight.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft);
    tfx::flight::ground_contact const ground{
      .present{present},
      .height{static_cast<int16_t>(height)},
    };
    tfx::flight::initialise_simple(state,position,angles,speed,landed,ground);
    if(entry >= 0) state.flight.fixed_speed = entry;
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int frame{0}; frame != frames; ++frame) {
      unsigned int ticks,buttons;int gear,engines;
      tfx::flight::simple_inputs controls{
        .source{static_cast<tfx::flight::control_source>(source)},
        .alt{rudder},
      };
      input >> ticks >> controls.joystick.x >> controls.joystick.y >> controls.keys.up >> controls.keys.left >> controls.keys.down >> controls.keys.right >> buttons >> controls.controls.accelerate >> controls.controls.decelerate >> gear >> engines;
      controls.controls.mouse_buttons = static_cast<uint8_t>(buttons);
      if(gear >= 0) {
        state.flight.gear_down = gear != 0;
        state.gear.condition = gear ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal;
      }
      if(engines >= 0) { state.flight.left_engine = engines & 1;state.flight.right_engine = engines & 2; }
      vec2<tfx::flight::system_condition> const engine_conditions{
        state.flight.left_engine ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal,
        state.flight.right_engine ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal,
      };
      if(disabled && frame >= 64) tfx::flight::advance_simple_disabled(state,controls.controls,engine_conditions,static_cast<tfx::simulation::timer_ticks>(ticks),ground);
      else tfx::flight::advance_simple(state,controls,engine_conditions,static_cast<tfx::simulation::timer_ticks>(ticks),ground);
      auto const values{words(state)};
      if(check) {
        for(auto const value : values) for(unsigned int shift{0}; shift != 64; shift += 8) digest = (digest ^ static_cast<uint8_t>(value >> shift)) * 1099511628211ULL;
      } else {
        for(auto const value : values) std::cout << value << ' ';
        std::cout << '\n';
      }
    }
    if(check) {
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native Simple flight sequence differs"};
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no Simple flight sequences"};
  if(check) std::cout << cases << " native Simple flight sequences agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
