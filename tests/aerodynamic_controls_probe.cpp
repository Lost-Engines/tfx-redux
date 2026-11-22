#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_controls.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits;
  input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic control fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int source;
  unsigned int afterburner;
  unsigned int control;
  unsigned int display;
  unsigned int cases{0};
  while(input >> source) {
    tfx::flight::aerodynamic_control_state state;
    tfx::flight::aerodynamic_powerplant powerplant;
    tfx::flight::aerodynamic_control_context context{
      .source{static_cast<tfx::flight::control_source>(source)},
    };
    input >> context.keys.up >> context.keys.left >> context.keys.down >> context.keys.right >> context.alt >> context.low_speed_ground_contact >> context.accelerate >> context.decelerate >> state.autopilot.enabled >> state.autopilot.mode >> state.autopilot.heading >> afterburner >> control >> display;
    powerplant.afterburner = static_cast<uint8_t>(afterburner);
    powerplant.afterburner_control = static_cast<uint8_t>(control);
    powerplant.afterburner_display = static_cast<uint8_t>(display);
    input >> context.mouse.x >> context.mouse.y >> context.joystick.x >> context.joystick.y;
    uint32_t throttle;
    input >> throttle;
    powerplant.throttle = std::bit_cast<float>(throttle);
    std::array<double *, 9> const fields{&state.elevator, &state.aileron, &state.rudder, &state.keyboard_elevator, &state.keyboard_aileron, &state.keyboard_rudder, &state.minimum_elevator, &state.maximum_elevator, &state.aileron_limit};
    for(auto *value : fields) *value = read_double(input);
    state.autopilot.down_metres = read_double(input);
    state.autopilot.metres_per_second = read_double(input);
    powerplant.engine_factor.x = read_double(input);
    powerplant.engine_factor.y = read_double(input);
    for(auto *value : {&context.seconds, &context.metres_per_second, &context.pitch_gain, &context.roll_gain, &context.rudder_increment, &context.joystick_gain, &context.elevator_trim, &context.maximum_autopilot_speed}) *value = read_double(input);
    bool const stopped{tfx::flight::update_aerodynamic_controls(state, powerplant, context)};
    std::vector<uint64_t> values;
    for(auto const *value : fields) values.push_back(std::bit_cast<uint64_t>(*value));
    for(auto const value : {state.autopilot.down_metres, state.autopilot.metres_per_second}) values.push_back(std::bit_cast<uint64_t>(value));
    values.push_back(state.autopilot.heading);
    values.push_back(std::bit_cast<uint32_t>(powerplant.throttle));
    for(auto const value : {powerplant.afterburner, powerplant.afterburner_control, powerplant.afterburner_display}) values.push_back(value);
    values.push_back(stopped);
    if(argc == 2) {
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic controls differ"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic control cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic control fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
