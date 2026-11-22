#pragma once

#include "flight/aerodynamic.h"

namespace tfx::flight {

enum class aerodynamic_pilot_key { cruise_speed, stick_pressure, waypoint, heading, track, autothrottle, toggle_autopilot };
enum class aerodynamic_pilot_notice { waypoint, heading, track, autothrottle, tracking, engaged, pressure_on, pressure_off };
enum class autopilot_external_status { unchanged, on, off };
struct aerodynamic_pilot_key_context {
  int16_t ground_elevation_steps{0};
  bool terrain_present{false};
  bool inhibited{false};
  bool ejected{false};
  bool recovery_available{false};
};
struct aerodynamic_pilot_key_events {
  std::optional<aerodynamic_pilot_notice> notice{};
  autopilot_external_status external_status{autopilot_external_status::unchanged}; // Native external "autopilo" on/off request.
  aerodynamic_recovery_result recovery{};
  bool motion_reset{false};                                                    // Native reset also updates the current timestep to 0.01 seconds.
};

// Native 6/5/7/8/9/0/A handlers. Held-arrow target changes belong to the device
// input law. A recovery-mode A press restores recovery through its own handler.
aerodynamic_pilot_key_events press_aerodynamic_pilot_key(aerodynamic_flight &state, damage_state &damage, bool &stick_pressure, aerodynamic_pilot_key key, aerodynamic_pilot_key_context const &context);

} // namespace tfx::flight
