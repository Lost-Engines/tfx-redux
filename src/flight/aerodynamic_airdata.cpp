#include "aerodynamic_airdata.h"
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr normalised_word{3.051850947599719e-05};
double constexpr native_pi{3.14159};
double constexpr degrees_per_radian{180.0 / native_pi}, angles_per_radian{32768.0 / native_pi};
double constexpr metres_to_kilometres{.001}, track_motion_threshold{.001};
double constexpr forward_angle_threshold{5};
float constexpr fixed_speed_scale{.4762499928474426f};

int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
calculation normalised(int32_t const value) {
  return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(value))) * normalised_word;
}

} // anonymous namespace

void update_aerodynamic_airdata(aerodynamic_airdata &state, aerodynamic_motion &motion, aerodynamic_airdata_context const &context) {
  for(unsigned int axis{0}; axis != 3; ++axis) {
    state.basis.forward[axis] = static_cast<double>(normalised(motion.pose.frame.forward[axis]));
    state.basis.right[axis] = static_cast<double>(normalised(motion.pose.frame.right[axis]));
    state.basis.down[axis] = static_cast<double>(normalised(motion.pose.frame.down[axis]));
  }
  calculation const downward{state.basis.forward.z};
  state.flight_path_pitch_degrees = static_cast<double>(degrees_per_radian * std::atan2(downward,std::sqrt(1.0 - downward * downward)));
  unsigned int local_axis{0};
  for(auto const *direction : {&state.basis.forward,&state.basis.right,&state.basis.down}) {
    motion.ground_velocity_body[local_axis] = static_cast<double>(static_cast<calculation>(direction->y) * motion.velocity_ned.y + static_cast<calculation>(direction->x) * motion.velocity_ned.x + static_cast<calculation>(direction->z) * motion.velocity_ned.z);
    vec3<calculation> const air_velocity{static_cast<calculation>(motion.velocity_ned.x) - context.wind_ned.x,static_cast<calculation>(motion.velocity_ned.y) - context.wind_ned.y,static_cast<calculation>(motion.velocity_ned.z) - context.wind_ned.z};
    motion.air_velocity_body[local_axis++] = static_cast<double>(static_cast<calculation>(direction->y) * air_velocity.y + static_cast<calculation>(direction->x) * air_velocity.x + static_cast<calculation>(direction->z) * air_velocity.z);
  }
  state.altitude_km = static_cast<double>(-static_cast<calculation>(motion.pose.metres_ned.z) * metres_to_kilometres);
  if((motion.velocity_ned.x > track_motion_threshold || motion.velocity_ned.x < -track_motion_threshold) && motion.ground_velocity_body.x > forward_angle_threshold) state.track_heading = truncate(std::atan2(motion.velocity_ned.y,motion.velocity_ned.x) * angles_per_radian);
  state.mass_kg = static_cast<double>(static_cast<calculation>(context.empty_mass_kg) + context.fuel_kg + context.stores_kg);
  if(motion.air_velocity_body.x > forward_angle_threshold) state.angle_of_attack = static_cast<double>(std::atan2(static_cast<calculation>(motion.air_velocity_body.z) / motion.air_velocity_body.x,1.0));
  state.atmosphere = sample_atmosphere(state.altitude_km,motion.pose.metres_ned.z,context.surface_celsius,context.atmosphere);
  vec3<calculation> const air_velocity{motion.air_velocity_body};
  // Native sqrt is called with a stored double argument.
  double const speed_squared{static_cast<double>(air_velocity.length_sq())};
  state.metres_per_second = static_cast<double>(std::sqrt(static_cast<calculation>(speed_squared)));
  state.fixed_speed = truncate(static_cast<calculation>(state.metres_per_second) / fixed_speed_scale);
  state.mach = static_cast<double>(static_cast<calculation>(state.metres_per_second) / state.atmosphere.sound_metres_per_second);
  state.simulation_seconds = static_cast<double>(static_cast<calculation>(context.seconds) + state.simulation_seconds);
  state.dynamic_pressure = static_cast<double>((static_cast<calculation>(state.metres_per_second) * state.metres_per_second) * (static_cast<calculation>(state.atmosphere.density) * .5));
}

} // namespace tfx::flight
