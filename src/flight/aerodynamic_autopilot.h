#pragma once

#include "flight/aerodynamic_navigation.h"

namespace tfx::flight {

struct aerodynamic_autopilot {
  aerodynamic_navigation navigation{};
  aerodynamic_guidance guidance{};
  aerodynamic_recovery_state recovery{};
  system_condition saved_air_brake{system_condition::normal};                  // Normal autopilot toggle, separate from recovery.
};
struct aerodynamic_autopilot_context {
  aerodynamic_guidance_context const &guidance;
  aerodynamic_navigation_context const &navigation;
};

// Complete native 0x9ac9c: low-lift speed recovery, manual/guided dispatch,
// navigation and guidance, then the common trim/limits/flap/nozzle suffix.
aerodynamic_guidance_events update_aerodynamic_autopilot(aerodynamic_autopilot &state, aerodynamic_manual_state &manual, aerodynamic_powerplant &powerplant, aerodynamic_motion &motion, aerodynamic_autopilot_context const &context);

} // namespace tfx::flight
