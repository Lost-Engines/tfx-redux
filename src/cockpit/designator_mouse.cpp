#include "designator_mouse.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace tfx::cockpit {

void designator_mouse::reset() { pending=remainder={0,0};wheel=0; }
void designator_mouse::move(vec2d const &delta) { pending+=delta; }
void designator_mouse::scroll(double offset) { wheel+=offset; }
void designator_mouse::apply(sensors::designator &state, bool tracking) {
  double const steps{std::trunc(wheel)};
  wheel-=steps;
  state.zoom=static_cast<uint8_t>(std::clamp(state.zoom+steps,0.0,10.0));
  auto const movement{std::exchange(pending,vec2d{0,0})};
  // Keep an existing designation: X explicitly releases it for free aiming.
  if(tracking) { remainder={0,0};return; }
  double constexpr units_per_pixel{.004*65536/(2*std::numbers::pi)};
  remainder+=movement*(units_per_pixel/(state.zoom+1));
  double const yaw{std::trunc(remainder.x)}, pitch{std::trunc(remainder.y)};
  remainder-=vec2d{yaw,pitch};
  state.heading=static_cast<math::angle>(state.heading+static_cast<int>(std::remainder(yaw,65536.0)));
  // Positive pitch points down. The original controller applies the F-117's
  // aircraft-relative limits next, then updates the ground point/zoom camera.
  state.pitch=static_cast<math::angle>(std::clamp(state.pitch+pitch,80.0,32767.0));
}

} // namespace tfx::cockpit
