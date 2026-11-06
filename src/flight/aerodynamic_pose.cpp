#include "aerodynamic_pose.h"
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr normalised_word{3.051850947599719e-05};
double constexpr world_units_per_metre{.4101049868766404};
double constexpr standard_eye_height{-2}, raptor_eye_height{-3};

int32_t truncate(calculation const value) {
  // Native FISTP truncation returns integer-indefinite for an invalid conversion.
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
calculation normalised(int32_t const component) {
  return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(component))) * normalised_word;
}

} // anonymous namespace

void aerodynamic_frame::normalise() {
  // The same native Q15 cyclic cross products apply to forward/right/down.
  math::attitude axes{
    .right{vec3<int32_t>{forward}},
    .down{vec3<int32_t>{right}},
    .forward{vec3<int32_t>{down}},
  };
  axes.normalise();
  forward = axes.right;
  right = axes.down;
  down = axes.forward;
}
math::attitude aerodynamic_frame::world_basis() const {
  // N/E/D become world Z/X/Y; local F/R/D become renderer F/R/D.
  return {
    .right{right.y,right.z,right.x},
    .down{down.y,down.z,down.x},
    .forward{forward.y,forward.z,forward.x},
  };
}
aerodynamic_view update_aerodynamic_view(aerodynamic_pose &state, player_aircraft const aircraft, aerodynamic_viewpoint const &viewpoint) {
  state.frame.normalise();
  aerodynamic_view result{
    .orientation{state.frame.world_basis()},
  };
  vec3d const metres_world{state.metres_ned.y,state.metres_ned.z,state.metres_ned.x};
  bool const standard_cockpit{aircraft == player_aircraft::eurofighter || aircraft == player_aircraft::nighthawk};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    // Native stores this sum as double before applying the upward offset.
    double const anchor{static_cast<double>(normalised(result.orientation.forward[axis]) * viewpoint.forward + static_cast<calculation>(metres_world[axis]) * world_units_per_metre)};
    calculation coordinate{anchor - normalised(result.orientation.down[axis]) * viewpoint.upward};
    if(axis == 1) coordinate += standard_cockpit ? standard_eye_height : raptor_eye_height;
    result.position[axis] = static_cast<float>(coordinate);
    result.integer_position[axis] = truncate(result.position[axis]);
  }
  result.angles = result.orientation.angles();
  return result;
}

} // namespace tfx::flight
