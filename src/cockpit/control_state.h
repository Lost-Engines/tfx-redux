#pragma once

#include "math/native.h"

namespace tfx::cockpit {

enum class autopilot_mode : uint16_t { waypoint, heading, track, autothrottle, recover };
enum class flight_configuration : uint16_t { combat, cruise, unlabelled, landing, refuel };

struct autopilot_display_state {
  bool enabled{false};
  autopilot_mode mode{autopilot_mode::waypoint};
  math::angle heading{0};
  int16_t waypoint{0};
  double height{0};                                                            // Down-positive metres, as stored by the native autopilot.
  double speed{0};                                                             // Metres per second.
};
struct control_display_state {
  double trailing_edge_flap{0};                                                // Native 0x14728.
  double leading_edge_flap{0};                                                 // Native 0x14720.
  vec2d thrust_vector{0,0};                                                    // Native 0x14840 / 0x14838; only the F-22 has thrust vectoring.
  autopilot_display_state autopilot;
  flight_configuration configuration{flight_configuration::combat};
};

} // namespace tfx::cockpit
