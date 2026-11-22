#pragma once

#include "flight/aerodynamic_recovery.h"

namespace tfx::flight {

struct aerodynamic_recovery_trigger_context {
  aerodynamic_recovery_context const &eligibility;
  double down_metres{0};
  double down_velocity{0};
  double metres_per_second{0};
  double gear_fraction{0};
  system_condition gear_condition{system_condition::normal};
};
struct aerodynamic_recovery_trigger_result {
  aerodynamic_recovery_result recovery{};
  bool descent_warning{false};                                                 // Native sound 0x3d and the vector PULL UP HUD shape at 0x6f12.
};

// Native 0x991e9..0x992f8: predict crossing zero physical height from current
// descent, warn or engage recovery. The original does not query terrain here.
aerodynamic_recovery_trigger_result check_aerodynamic_recovery(aerodynamic_recovery_state &state, aerodynamic_control_state &controls, aerodynamic_powerplant &powerplant, system_condition &air_brake, aerodynamic_recovery_trigger_context const &context);

} // namespace tfx::flight
