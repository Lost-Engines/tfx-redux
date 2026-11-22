#pragma once

#include <vector>
#include "flight/aerodynamic.h"

namespace tfx::flight {

enum class aerodynamic_callback { configured, military, realistic };

struct aerodynamic_tick_input {
  control_source source{control_source::keyboard};
  direction_keys keys{};
  vec2<int16_t> joystick{0,0};
  vec2<int16_t> mouse{0,0};
  bool alt{false};
  bool accelerate{false};
  bool decelerate{false};
};
struct aerodynamic_tick_context {
  aerodynamic_tick_input input{};
  double seconds{.02};
  int16_t ground_elevation_steps{0};
  std::optional<int16_t> surface_celsius{};
  uint8_t refuelling_phase{0};
  bool terrain_present{false};
  bool rough_surface{false};
  bool carrier_surface{false};
  bool wheel_brakes{false};
  bool gear_damage_enabled{true};
  bool precipitation{false};
  bool time_warp{false};
  bool crashing{true};
  bool stick_pressure{false};
  bool wind_enabled{false};
  bool terrain_wind_boost{false};
  bool weather_wind_boost{false};
  aerodynamic_navigation_context navigation{};
  bool recovery_available{false};
  bool recovery_inhibited{false};
  bool ejected{false};
  bool approach_reference_enabled{false};
  aerodynamic_callback callback{aerodynamic_callback::configured};
};
struct aerodynamic_tick_events {
  std::vector<afterburner_change> afterburner{};
  aerodynamic_ground_events ground{};
  std::vector<aerodynamic_recovery_change> recovery_changes{};
  bool descent_warning{false};
};

// Joined 0x9fe14 / 0x9fe50 callbacks, including manual/autopilot control.
// Automatic recovery and entry-time model resets are included; automatic
// landing remains a separate callback. The outer caller must acknowledge the
// prior-model marker after this callback, as native 0x8f98c does.
aerodynamic_tick_events advance_aerodynamic_flight(aerodynamic_flight &state, damage_state &damage, math::random_state &random, aerodynamic_wind_parameters const &wind_parameters, aerodynamic_tick_context const &context);

} // namespace tfx::flight
