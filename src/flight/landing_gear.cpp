#include "landing_gear.h"

namespace tfx::flight {

std::vector<unsigned int> update_landing_gear(landing_gear_state &state, landing_gear_context const &context) {
  std::vector<unsigned int> sounds;
  if(!state.down) return sounds;
  using calculation = double;
  float constexpr normalised_word{3.0518509447574615e-05f};
  double constexpr touchdown_failure_multiplier{2}, overspeed_failure_multiplier{1.4};
  unsigned int constexpr failure_sound{0x3f}, warning_sound{0x3a};
  auto const fail{[&] {
    sounds.push_back(failure_sound);
    state.condition = system_condition::failed;
    state.down = false;
  }};
  state.vertical_speed = static_cast<float>(static_cast<calculation>(context.forward_y) * normalised_word * context.metres_per_second);
  if(context.landed) {
    if(state.vertical_speed > static_cast<calculation>(context.touchdown_descent_limit) * touchdown_failure_multiplier && !context.previous_landed) { fail();return sounds; }
    if(state.vertical_speed > context.touchdown_descent_limit && state.condition != system_condition::failed && !context.previous_landed) state.condition = system_condition::intermittent;
    if(static_cast<calculation>(context.ground_speed_limit) * overspeed_failure_multiplier < context.metres_per_second) { fail();return sounds; }
    if(context.ground_speed_limit < context.metres_per_second) {
      sounds.push_back(warning_sound);
      state.condition = system_condition::intermittent;
    }
  }
  if(static_cast<calculation>(context.airborne_speed_limit) * overspeed_failure_multiplier < context.metres_per_second) { fail();return sounds; }
  if(context.airborne_speed_limit < context.metres_per_second) {
    sounds.push_back(warning_sound);
    state.condition = system_condition::intermittent;
  }
  return sounds;
}

} // namespace tfx::flight
