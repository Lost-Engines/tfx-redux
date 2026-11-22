#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>
#include "flight/aerodynamic_recovery_trigger.h"

int main(int const argc, char const *const argv[]) try {
  bool const descent{argc >= 2 && std::string_view{argv[1]} == "--descent"};
  char const *path{nullptr};
  if(descent && argc == 3) {
    path = argv[2];
  } else if(!descent && argc == 2) path = argv[1];
  if(argc > (descent ? 3 : 2)) throw std::runtime_error{"expected [--descent] [FILE]"};
  std::ifstream fixture;
  if(path) fixture.open(path);
  if(path && !fixture) throw std::runtime_error{"cannot open aerodynamic recovery fixtures"};
  auto &input{path ? fixture : std::cin};
  unsigned int available;
  unsigned int inhibited;
  unsigned int crashed;
  unsigned int ejected;
  unsigned int landed;
  unsigned int stages;
  unsigned int configuration;
  unsigned int saved_configuration;
  unsigned int mode;
  unsigned int saved_mode;
  unsigned int enabled;
  unsigned int saved_enabled;
  unsigned int brake;
  unsigned int saved_brake;
  unsigned int afterburner;
  unsigned int saved_afterburner;
  unsigned int control;
  unsigned int display;
  unsigned int pending;
  unsigned int throttle;
  unsigned int cases{0};
  uint64_t saved_throttle;
  uint64_t target;
  uint64_t left;
  uint64_t right;
  uint64_t fuel;
  while(input >> available >> inhibited >> crashed >> ejected >> landed >> stages >> configuration >> saved_configuration >> mode >> saved_mode >> enabled >> saved_enabled >> brake >> saved_brake >> afterburner >> saved_afterburner >> control >> display >> pending >> throttle >> saved_throttle >> target >> left >> right >> fuel) {
    tfx::flight::aerodynamic_recovery_state state{
      .configuration{static_cast<uint16_t>(configuration)},
      .saved_configuration{static_cast<uint16_t>(saved_configuration)},
      .saved_mode{static_cast<uint16_t>(saved_mode)},
      .saved_enabled{saved_enabled != 0},
      .saved_air_brake{static_cast<tfx::flight::system_condition>(saved_brake)},
      .saved_throttle{std::bit_cast<double>(saved_throttle)},
      .saved_afterburner{static_cast<uint8_t>(saved_afterburner)},
      .model_reset_requested{pending != 0},
    };
    tfx::flight::aerodynamic_control_state controls;
    controls.autopilot.enabled = enabled != 0;
    controls.autopilot.mode = static_cast<uint16_t>(mode);
    auto air_brake{static_cast<tfx::flight::system_condition>(brake)};
    tfx::flight::aerodynamic_powerplant powerplant{
      .engine_factor{std::bit_cast<double>(left), std::bit_cast<double>(right)},
      .throttle{std::bit_cast<float>(throttle)},
      .throttle_target{std::bit_cast<double>(target)},
      .fuel{std::bit_cast<double>(fuel)},
      .afterburner{static_cast<uint8_t>(afterburner)},
      .afterburner_control{static_cast<uint8_t>(control)},
      .afterburner_display{static_cast<uint8_t>(display)},
    };
    tfx::flight::aerodynamic_recovery_context const context{
      .afterburner_stages{stages},
      .available{available != 0},
      .inhibited{inhibited != 0},
      .crashed{crashed != 0},
      .ejected{ejected != 0},
      .landed{landed != 0},
    };
    tfx::flight::aerodynamic_recovery_result result;
    bool warning{false};
    if(descent) {
      uint64_t down;
      uint64_t velocity;
      uint64_t speed;
      uint64_t gear;
      unsigned int condition;
      input >> down >> velocity >> speed >> gear >> condition;
      auto const predicted{tfx::flight::check_aerodynamic_recovery(state, controls, powerplant, air_brake, {
        .eligibility{context},
        .down_metres{std::bit_cast<double>(down)},
        .down_velocity{std::bit_cast<double>(velocity)},
        .metres_per_second{std::bit_cast<double>(speed)},
        .gear_fraction{std::bit_cast<double>(gear)},
        .gear_condition{static_cast<tfx::flight::system_condition>(condition)},
      })};
      result = predicted.recovery;
      warning = predicted.descent_warning;
    } else {
      result = tfx::flight::toggle_aerodynamic_recovery(state, controls, powerplant, air_brake, context);
    }
    std::vector<uint64_t> values{state.configuration, state.saved_configuration, state.saved_mode, state.saved_enabled, static_cast<unsigned int>(state.saved_air_brake), std::bit_cast<uint64_t>(state.saved_throttle), state.saved_afterburner, state.model_reset_requested, controls.autopilot.enabled, controls.autopilot.mode, static_cast<unsigned int>(air_brake), std::bit_cast<uint32_t>(powerplant.throttle), std::bit_cast<uint64_t>(powerplant.throttle_target), powerplant.afterburner, powerplant.afterburner_control, powerplant.afterburner_display, static_cast<unsigned int>(result.change), result.afterburner.size()};
    for(auto const change : result.afterburner) values.push_back(static_cast<unsigned int>(change));
    if(descent) values.push_back(warning);
    if(path) {
      for(auto const value : values) {
        uint64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native aerodynamic recovery differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic recovery cases"};
  if(path) std::cout << cases << " native aerodynamic recovery fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
