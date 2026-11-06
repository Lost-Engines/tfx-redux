#include "automatic_approach.h"
#include <bit>
#include <cmath>
#include <limits>
#include "flight/aerodynamic_rotation_internal.h"

namespace tfx::flight {
namespace {
using calculation = double;
double constexpr kilometres_per_metre{.001}, normalised_word{3.051850947599719e-05};
double constexpr units_per_half_turn{32768}, inverse_degrees{.005555555555555555}, native_pi{3.14159};
double constexpr carrier_height_steps{20}, metres_per_foot{.3048}, feet_per_height_step{8};
double constexpr glide_degrees{7}, metres_per_kilometre{1000}, approach_height_bias{1};
double constexpr flare_height{-25}, maximum_flare_pitch{20}, pitch_step{1}, runway_speed_step{-.5}, carrier_speed_step{-2.5};
double constexpr glide_radians{native_pi * glide_degrees * inverse_degrees};
int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
int32_t subtract(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
} // anonymous namespace

bool advance_automatic_approach(automatic_approach &state, aerodynamic_motion &motion, aerodynamic_basis &basis, double &metres_per_second, automatic_approach_context const &context) {
  state.player_metres = {truncate(motion.pose.metres_ned.y),truncate(motion.pose.metres_ned.z),truncate(motion.pose.metres_ned.x)};
  calculation const north{static_cast<calculation>(subtract(state.destination_metres.z,state.player_metres.z))};
  calculation const east{static_cast<calculation>(subtract(state.destination_metres.x,state.player_metres.x))};
  state.distance_km = static_cast<double>(std::sqrt(north * north + east * east) * kilometres_per_metre);
  motion.pose.frame = {};
  rotate_aerodynamic_units(motion,aerodynamic_rotation::yaw,static_cast<double>(math::signed_word(context.runway_heading) + units_per_half_turn));
  rotate_aerodynamic_units(motion,aerodynamic_rotation::pitch,static_cast<double>(static_cast<calculation>(state.pitch_degrees) * units_per_half_turn * inverse_degrees));
  for(unsigned int axis{0}; axis != 3; ++axis) {
    basis.forward[axis] = static_cast<double>(static_cast<calculation>(math::signed_word(static_cast<uint16_t>(motion.pose.frame.forward[axis]))) * normalised_word);
    basis.right[axis] = static_cast<double>(static_cast<calculation>(math::signed_word(static_cast<uint16_t>(motion.pose.frame.right[axis]))) * normalised_word);
    basis.down[axis] = static_cast<double>(static_cast<calculation>(math::signed_word(static_cast<uint16_t>(motion.pose.frame.down[axis]))) * normalised_word);
  }
  calculation const travel{static_cast<calculation>(metres_per_second) * context.seconds};
  motion.pose.metres_ned.x = static_cast<double>(travel * basis.forward.x + motion.pose.metres_ned.x);
  motion.pose.metres_ned.y = static_cast<double>(travel * basis.forward.y + motion.pose.metres_ned.y);
  double const deck_height{static_cast<double>(static_cast<calculation>(context.carrier ? carrier_height_steps : 0) * metres_per_foot * feet_per_height_step)};
  if(-static_cast<calculation>(context.left_main_wheel_down) - deck_height > motion.pose.metres_ned.z && !state.flaring) {
    motion.pose.metres_ned.z = static_cast<double>(approach_height_bias - static_cast<calculation>(state.distance_km) * metres_per_kilometre * std::sin(glide_radians) - deck_height);
    if(static_cast<calculation>(flare_height) - deck_height < motion.pose.metres_ned.z && state.pitch_degrees < maximum_flare_pitch) state.pitch_degrees = static_cast<double>(static_cast<calculation>(state.pitch_degrees) + pitch_step);
    return false;
  }
  state.flaring = true;
  if(state.pitch_degrees > 0) state.pitch_degrees = static_cast<double>(static_cast<calculation>(state.pitch_degrees) - pitch_step);
  motion.pose.metres_ned.z = static_cast<double>(-static_cast<calculation>(context.left_main_wheel_down) - deck_height);
  metres_per_second = static_cast<double>(static_cast<calculation>(metres_per_second) + (context.carrier ? carrier_speed_step : runway_speed_step));
  return !(metres_per_second > 0);
}

} // namespace tfx::flight
