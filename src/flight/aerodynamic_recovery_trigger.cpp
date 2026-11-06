#include "aerodynamic_recovery_trigger.h"

namespace tfx::flight {
namespace {
uint16_t constexpr recovery_mode{4};
double constexpr minimum_descent{.1}, autopilot_recovery_seconds{8}, urgent_recovery_seconds{5}, warning_seconds{7}, damaged_gear_recovery_speed{200};
} // anonymous namespace

aerodynamic_recovery_trigger_result check_aerodynamic_recovery(aerodynamic_recovery_state &state, aerodynamic_control_state &controls, aerodynamic_powerplant &powerplant, system_condition &air_brake, aerodynamic_recovery_trigger_context const &context) {
  aerodynamic_recovery_trigger_result result;
  auto const &targets{controls.autopilot};
  if(targets.mode == recovery_mode || context.gear_fraction != 0 || !(context.down_velocity > minimum_descent)) return result;
  double const crossing_seconds{static_cast<double>(static_cast<double>(context.down_metres) / -static_cast<double>(context.down_velocity))};
  if(!(crossing_seconds > 0)) return result;
  if((crossing_seconds < autopilot_recovery_seconds && targets.enabled) || (crossing_seconds < urgent_recovery_seconds && (context.gear_condition == system_condition::normal || context.metres_per_second > damaged_gear_recovery_speed))) {
    result.recovery = toggle_aerodynamic_recovery(state,controls,powerplant,air_brake,context.eligibility);
    return result;
  }
  result.descent_warning = crossing_seconds < warning_seconds && crossing_seconds >= urgent_recovery_seconds;
  return result;
}

} // namespace tfx::flight
