#pragma once

#include "flight/aerodynamic_forces.h"

namespace tfx::flight {

struct automatic_approach {
  vec3<int32_t> destination_metres{0,0,0};                                     // World-axis order: East/Down/North.
  vec3<int32_t> player_metres{0,0,0};
  double distance_km{0};
  double pitch_degrees{-7};
  bool flaring{false};
};
struct automatic_approach_context {
  math::angle runway_heading{0};
  double seconds{.02};
  double left_main_wheel_down{0};
  bool carrier{false};
};

// Native 0x9fecf..0xa028f: scripted approach/flare/rollout. The returned stop
// request precedes the separate touchdown finalizer, gear and view-export tail.
bool advance_automatic_approach(automatic_approach &state, aerodynamic_motion &motion, aerodynamic_basis &basis, double &metres_per_second, automatic_approach_context const &context);

} // namespace tfx::flight
