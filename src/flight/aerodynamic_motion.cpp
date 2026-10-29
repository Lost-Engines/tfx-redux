#include "aerodynamic_rotation_internal.h"
#include <bit>
#include <cmath>
#include "math/floating_environment.h"
#include <limits>

namespace tfx::flight {
namespace {

using calculation = long double;
double constexpr half_step{.5}, native_pi{3.141592654};
double constexpr angle_units_per_half_turn{32768};
unsigned int constexpr time_warp_multiplier{5};
double constexpr impact_down_metres{2};
int32_t constexpr fallback_height_limit{-2}, fallback_world_y{-5};

int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0L) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
math::angle rotation_amount(double const amount, double *const remainder) {
  calculation const sum{static_cast<calculation>(amount) + (remainder ? *remainder : 0)};
  if(remainder) *remainder = static_cast<double>(sum);
  auto const result{static_cast<math::angle>(truncate(sum))};
  if(remainder) *remainder = static_cast<double>(static_cast<calculation>(*remainder) - math::signed_word(result));
  return result;
}
void rotate(aerodynamic_motion &state, calculation const roll, calculation const pitch, calculation const yaw) {
  double const inverse_pi{static_cast<double>(1.0L / native_pi)};
  rotate_aerodynamic_units(state,aerodynamic_rotation::roll,static_cast<double>(roll * angle_units_per_half_turn * inverse_pi));
  rotate_aerodynamic_units(state,aerodynamic_rotation::pitch,static_cast<double>(pitch * angle_units_per_half_turn * inverse_pi));
  rotate_aerodynamic_units(state,aerodynamic_rotation::yaw,static_cast<double>(yaw * angle_units_per_half_turn * inverse_pi));
}

} // anonymous namespace

void rotate_aerodynamic_units(aerodynamic_motion &state, aerodynamic_rotation const axis, double const units) {
  math::floating_environment const environment;
  // Physical F/R/D columns map to the generic first/second/third columns.
  math::attitude frame{
    .right{vec3<int32_t>{state.pose.frame.forward}},
    .down{vec3<int32_t>{state.pose.frame.right}},
    .forward{vec3<int32_t>{state.pose.frame.down}},
  };
  double *const remainder{axis == aerodynamic_rotation::roll ? &state.rotation_remainder.x : axis == aerodynamic_rotation::pitch ? &state.rotation_remainder.y : nullptr};
  auto const rotation{axis == aerodynamic_rotation::roll ? math::rotation_axis::pitch : axis == aerodynamic_rotation::pitch ? math::rotation_axis::heading : math::rotation_axis::roll};
  frame.rotate(rotation,rotation_amount(units,remainder));
  state.pose.frame.forward = frame.right;
  state.pose.frame.right = frame.down;
  state.pose.frame.down = frame.forward;
}

void advance_aerodynamic_motion(aerodynamic_motion &state, aerodynamic_motion_context const &context) {
  math::floating_environment const environment;
  calculation const seconds{context.seconds};
  unsigned int const multiplier{context.time_warp && !context.landed ? time_warp_multiplier : 1};
  calculation down{0};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const displacement{static_cast<double>(static_cast<calculation>(state.velocity_ned[axis]) * seconds - static_cast<calculation>(state.acceleration_ned[axis]) * seconds * seconds * half_step)};
    auto const coordinate{static_cast<calculation>(displacement) * multiplier + state.pose.metres_ned[axis]};
    state.pose.metres_ned[axis] = static_cast<double>(coordinate);
    if(axis == 2) down = coordinate;
  }
  if(down > impact_down_metres) state.collision = 1;
  if(!context.crashing && state.collision) {
    state.collision = 0;
    if(state.velocity_ned.z > 0) state.velocity_ned.z = std::bit_cast<double>(std::bit_cast<uint64_t>(state.velocity_ned.z) ^ (uint64_t{1} << 63));
    if(state.integer_world_y > fallback_height_limit) state.integer_world_y = fallback_world_y;
  }
  if(context.simplified_rotation) {
    state.angular_rate.x = context.roll_rate_inhibited ? 0 : context.commanded_roll_rate;
    state.angular_rate.y = static_cast<double>((static_cast<calculation>(context.commanded_angle_of_attack) - context.angle_of_attack) / seconds);
    // The original still rotates by the command when the stored roll rate is inhibited.
    rotate(state,-seconds * context.commanded_roll_rate,static_cast<calculation>(context.commanded_angle_of_attack) - context.angle_of_attack,-static_cast<calculation>(state.yaw_step_radians));
  } else {
    state.angular_rate.z = static_cast<double>(static_cast<calculation>(state.yaw_step_radians) / seconds);
    rotate(state,-seconds * (state.angular_rate.x - seconds * half_step * state.angular_acceleration.x),seconds * (state.angular_rate.y - seconds * half_step * state.angular_acceleration.y),-static_cast<calculation>(state.yaw_step_radians));
  }
}

} // namespace tfx::flight
