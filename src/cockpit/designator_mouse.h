#pragma once

#include "sensors/ground.h"

namespace tfx::cockpit {

// Relative camera input, independent of flight controls and render/update rate.
class designator_mouse {
  vec2d pending{0,0}, remainder{0,0};
  double wheel{0};
public:
  void reset();
  void move(vec2d const &delta);
  void scroll(double offset);
  void apply(sensors::designator &state, bool tracking);
};

} // namespace tfx::cockpit
