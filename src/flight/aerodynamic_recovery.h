#pragma once

#include <vector>
#include "flight/aerodynamic_controls.h"

namespace tfx::flight {

struct aerodynamic_recovery_state {
  uint16_t configuration{1};                                                   // Native control configuration (landing also owns codes).
  uint16_t saved_configuration{0};
  uint16_t saved_mode{0};
  bool saved_enabled{false};
  system_condition saved_air_brake{system_condition::normal};
  double saved_throttle{0};
  uint8_t saved_afterburner{0};
  bool model_reset_requested{false};
};
struct aerodynamic_recovery_context {
  unsigned int afterburner_stages{0};
  bool available{false};
  bool inhibited{false};
  bool crashed{false};
  bool ejected{false};
  bool landed{false};
};
enum class aerodynamic_recovery_change {
  none,
  engaged,
  restored
};
struct aerodynamic_recovery_result {
  aerodynamic_recovery_change change{aerodynamic_recovery_change::none};
  std::vector<afterburner_change> afterburner{};
};

// Complete 0x85a7c. Engagement returns its sound/radio trigger; restoration
// retains ordered original afterburner-handler requests and physical reset.
aerodynamic_recovery_result toggle_aerodynamic_recovery(aerodynamic_recovery_state &state, aerodynamic_control_state &controls, aerodynamic_powerplant &powerplant, system_condition &air_brake, aerodynamic_recovery_context const &context);

} // namespace tfx::flight
