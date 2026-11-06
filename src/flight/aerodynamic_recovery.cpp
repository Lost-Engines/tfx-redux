#include "aerodynamic_recovery.h"
#include <stdexcept>

namespace tfx::flight {
namespace {

uint16_t constexpr recovery_mode{4}, recovery_configuration{5};

} // anonymous namespace

aerodynamic_recovery_result toggle_aerodynamic_recovery(aerodynamic_recovery_state &state, aerodynamic_control_state &controls, aerodynamic_powerplant &powerplant, system_condition &air_brake, aerodynamic_recovery_context const &context) {
  aerodynamic_recovery_result result;
  if(context.inhibited || context.crashed || context.ejected || context.landed) return result;
  auto &autopilot{controls.autopilot};
  if(autopilot.mode != recovery_mode) {
    if(!context.available) return result;
    state.saved_configuration = state.configuration;
    state.saved_enabled = autopilot.enabled;
    state.saved_mode = autopilot.mode;
    state.saved_air_brake = air_brake;
    state.saved_throttle = powerplant.throttle;
    state.saved_afterburner = powerplant.afterburner;
    state.configuration = recovery_configuration;
    autopilot.enabled = true;
    autopilot.mode = recovery_mode;
    state.model_reset_requested = true;
    result.change = aerodynamic_recovery_change::engaged;
    return result;
  }
  state.configuration = state.saved_configuration;
  autopilot.enabled = state.saved_enabled;
  autopilot.mode = state.saved_mode;
  air_brake = state.saved_air_brake;
  powerplant.throttle = static_cast<float>(state.saved_throttle);
  state.model_reset_requested = true;
  while(powerplant.afterburner != state.saved_afterburner) {
    auto const change{powerplant.afterburner < state.saved_afterburner ? powerplant.increase_afterburner(context.afterburner_stages) : powerplant.decrease_afterburner()};
    // The native loop cannot terminate if the saved reheat is now unavailable.
    // Diagnose that state rather than entering its original infinite loop.
    if(change == afterburner_change::none) throw std::runtime_error{"cannot restore unavailable recovery afterburner state"};
    result.afterburner.push_back(change);
  }
  result.change = aerodynamic_recovery_change::restored;
  return result;
}

} // namespace tfx::flight
