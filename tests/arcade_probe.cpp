#include "fixture_check.h"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <stdexcept>
#include "flight/arcade.h"

namespace {

void output(tfx::flight::arcade_state const &state, std::ostream &stream, bool const ground) {
  auto const floating{[&stream](float const value){ stream << std::bit_cast<uint32_t>(value) << ' '; }};
  auto const precise{[&stream](double const value){ stream << std::bit_cast<uint64_t>(value) << ' '; }};
  for(auto const value : {state.position.x,state.position.y,state.position.z}) floating(value);
  for(unsigned int axis{0}; axis != 3; ++axis) stream << state.integer_position[axis] << ' ';
  for(unsigned int row{0}; row != 3; ++row) stream << state.orientation.right[row] << ' ' << state.orientation.down[row] << ' ' << state.orientation.forward[row] << ' ';
  for(auto const value : {state.rotation_remainder.x,state.rotation_remainder.y,state.rotation_remainder.z,state.control_target.x,state.control_target.y,state.filtered_control.x,state.filtered_control.y,state.speed_mach,state.throttle,state.minimum_speed,state.maximum_speed,state.target_speed,state.speed_metres_per_second,state.altitude_km,state.movement_per_tick}) floating(value);
  precise(state.unrounded_speed_mach);
  stream << state.fixed_speed << ' ' << state.landed << ' ' << state.low_speed_ground_contact << ' ' << state.collision << ' ' << state.recovering << ' ' << static_cast<unsigned int>(state.afterburner) << ' ' << state.fuel_readout << ' ';
  precise(state.fuel);
  precise(state.fuel_flow);
  stream << state.angles.x << ' ' << state.angles.y << ' ' << state.angles.z;
  if(ground) stream << ' ' << state.gear_down << ' ' << state.brake_chute;
  stream << '\n';
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(std::numeric_limits<long double>::digits != 64) return 77;
  bool const disabled{argc > 1 && (std::string_view{argv[1]} == "--disabled" || std::string_view{argv[1]} == "--check-disabled")};
  bool const ground_mode{argc > 1 && (std::string{argv[1]} == "--ground" || std::string{argv[1]} == "--check-ground")};
  bool const check{argc == 3 && (std::string{argv[1]} == "--check" || std::string{argv[1]} == "--check-ground" || disabled)};
  if(argc != 1 && !check && !(argc == 2 && (ground_mode || disabled))) throw std::runtime_error{"expected --check FILE or standard-input trace"};
  std::ifstream fixture;
  if(check) {
    fixture.open(argv[2]);
    if(!fixture) throw std::runtime_error{"cannot read Arcade reference fixture"};
  }
  std::istream &input{check ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int aircraft{0}, frames{0};
  while(input >> aircraft >> frames) {
    vec3<int32_t> position;
    vec3<tfx::math::angle> angles;
    float speed{0};
    input >> position.x >> position.y >> position.z >> angles.x >> angles.y >> angles.z >> speed;
    tfx::flight::arcade_state state;
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft);
    unsigned int landed{0}, engines{0};
    int height{0};
    if(ground_mode) input >> landed >> height >> engines;
    tfx::flight::ground_contact const ground{
      .present{ground_mode},
      .height{static_cast<int16_t>(height)},
    };
    tfx::flight::initialise_arcade(state,position,angles,speed,landed != 0,ground);
    if(ground_mode) { state.left_engine = engines != 0; state.right_engine = engines != 0; }
    uint64_t hash{14695981039346656037ULL};
    for(unsigned int frame{0}; frame != frames; ++frame) {
      tfx::flight::flight_controls controls;
      unsigned int ticks{0}, buttons{0}, accelerate{0}, decelerate{0};
      if(!(input >> ticks >> controls.pitch_delta >> controls.roll_delta >> buttons >> accelerate >> decelerate)) throw std::runtime_error{"truncated arcade control trace"};
      controls.mouse_buttons = static_cast<uint8_t>(buttons);
      controls.accelerate = accelerate != 0;
      controls.decelerate = decelerate != 0;
      vec2<tfx::flight::system_condition> const engine_conditions{
        state.left_engine ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal,
        state.right_engine ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal,
      };
      if(disabled && frame >= 64) tfx::flight::advance_disabled(state,controls,engine_conditions,static_cast<uint16_t>(ticks),ground);
      else tfx::flight::advance_arcade(state,controls,engine_conditions,static_cast<uint16_t>(ticks),ground);
      std::ostringstream row;
      output(state,row,ground_mode);
      if(check) for(unsigned char const byte : row.str()) hash = (hash ^ byte) * 1099511628211ULL;
      else std::cout << row.str();
    }
    if(check) {
      if(!tfx::test::fixture_matches(input,hash)) throw std::runtime_error{"Arcade native trace differs in case " + std::to_string(cases)};
    }
    ++cases;
  }
  if(check && !cases) throw std::runtime_error{"empty Arcade reference fixture"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
