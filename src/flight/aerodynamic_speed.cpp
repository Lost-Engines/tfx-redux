#include "aerodynamic_speed.h"

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr speed_response{.01}, force_response{25}, idle_throttle{.6};
double constexpr decrease_afterburner_delta{-10}, increase_afterburner_delta{20};
double constexpr air_brake_overspeed{5}, recovery_speed_multiplier{2};

void adjust_throttle(aerodynamic_speed_state const &state, aerodynamic_powerplant &powerplant, aerodynamic_speed_context const &context) {
  powerplant.throttle = static_cast<float>((static_cast<calculation>(state.target_metres_per_second) - context.metres_per_second) * speed_response + powerplant.throttle);
  powerplant.throttle = static_cast<float>(powerplant.throttle - static_cast<calculation>(context.forward_force) / (static_cast<calculation>(context.mass_kg) * force_response));
}

} // anonymous namespace

afterburner_change update_aerodynamic_speed(aerodynamic_speed_state &state, aerodynamic_powerplant &powerplant, aerodynamic_speed_context const &context) {
  afterburner_change change{afterburner_change::none};
  bool const running{powerplant.engine_factor.x != 0 || powerplant.engine_factor.y != 0};
  if(context.strategy == aerodynamic_speed_strategy::maintain) {
    if(running) {
      if(!powerplant.afterburner) adjust_throttle(state,powerplant,context);
      if(powerplant.throttle < idle_throttle) powerplant.throttle = static_cast<float>(idle_throttle);
      if(powerplant.afterburner) {
        powerplant.throttle = 1;
        if(static_cast<calculation>(state.target_metres_per_second) - context.metres_per_second < decrease_afterburner_delta) change = powerplant.decrease_afterburner();
      }
    }
    bool const brake{static_cast<calculation>(context.metres_per_second) - state.target_metres_per_second > air_brake_overspeed && context.air_brake_drag != 0};
    state.air_brake_fraction = brake ? 1 : 0;
    state.air_brake_condition = brake ? system_condition::active : system_condition::normal;
  } else {
    if(running) {
      double const target{state.target_metres_per_second};
      if(target < context.metres_per_second) state.target_metres_per_second = static_cast<double>(static_cast<calculation>(context.metres_per_second) * recovery_speed_multiplier);
      if(!powerplant.afterburner) adjust_throttle(state,powerplant,context);
      if(powerplant.throttle < idle_throttle) powerplant.throttle = static_cast<float>(idle_throttle);
      if(powerplant.throttle >= 1) {
        powerplant.throttle = 1;
        if(static_cast<calculation>(state.target_metres_per_second) - context.metres_per_second > increase_afterburner_delta) change = powerplant.increase_afterburner(context.afterburner_stages);
      }
      state.target_metres_per_second = target;
    }
    state.air_brake_fraction = 0;
    state.air_brake_condition = system_condition::normal;
  }
  return change;
}

} // namespace tfx::flight
