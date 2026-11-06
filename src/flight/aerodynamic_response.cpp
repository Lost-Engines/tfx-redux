#include "aerodynamic_response.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr rolling_speed{30}, sideslip_speed{1}, minimum_damping_speed{10};
double constexpr warp_acceleration_altitude{-20000}, warp_multiplier{5};
double constexpr rudder_scale{.004878048780487805}, rolling_rudder_scale{1.0 / 6};
double constexpr fuel_roll_inertia{.004166666666666667};
// Native inverse-degrees literal is one double ULP below rounded 1/180.
double constexpr native_pi{3.14159}, inverse_degrees_per_half_turn{.005555555555555555};
double constexpr engine_angle{native_pi * 20 * inverse_degrees_per_half_turn};

} // anonymous namespace

void update_aerodynamic_response(aerodynamic_response_state &state, aerodynamic_response_context const &context) {
  auto &motion{state.motion};
  auto const &basis{context.basis};
  auto const &p{context.parameters};
  auto const &inertia{context.inertia};
  calculation const seconds{context.seconds}, speed{context.metres_per_second};
  if(motion.air_velocity_body.x > sideslip_speed || motion.air_velocity_body.x < -sideslip_speed) state.sideslip_radians = static_cast<double>(std::atan2(motion.air_velocity_body.y,motion.air_velocity_body.x));
  bool low_speed_ground{false};
  if(!context.landed) motion.yaw_step_radians = state.sideslip_radians;
  else {
    low_speed_ground = motion.ground_velocity_body.x < rolling_speed;
    state.sideslip_radians = 0;
    motion.yaw_step_radians = 0;
    if(context.ground_vertical_force != 0) motion.air_velocity_body.y = 0;
  }
  // The native velocity warp is gated at 20 km and by autopilot, separately
  // from the always-airborne positional warp in the later motion callback.
  double const velocity_seconds{context.time_warp && !context.landed && !context.autopilot && motion.pose.metres_ned.z < warp_acceleration_altitude ? static_cast<double>(seconds * warp_multiplier) : context.seconds};
  for(unsigned int axis{0}; axis != 3; ++axis) motion.velocity_ned[axis] = static_cast<double>(static_cast<calculation>(motion.acceleration_ned[axis]) * velocity_seconds + motion.velocity_ned[axis]);
  if(context.rudder != 0 && !context.rudder_inhibited) {
    auto const projection{[&](vec3d const &direction) {
      return static_cast<double>(static_cast<calculation>(direction.y) * motion.velocity_ned.y + static_cast<calculation>(direction.x) * motion.velocity_ned.x + static_cast<calculation>(direction.z) * motion.velocity_ned.z);
    }};
    motion.ground_velocity_body.x = projection(basis.forward);
    motion.ground_velocity_body.z = projection(basis.down);
    motion.ground_velocity_body.y = 0;
    motion.yaw_step_radians = static_cast<double>(speed * rudder_scale * p.rudder_effectiveness * seconds * context.rudder + motion.yaw_step_radians);
    if(low_speed_ground) motion.yaw_step_radians = static_cast<double>(speed / rolling_speed * p.rudder_effectiveness * rolling_rudder_scale * context.rudder);
    for(unsigned int axis{0}; axis != 3; ++axis) motion.velocity_ned[axis] = static_cast<double>(static_cast<calculation>(basis.down[axis]) * motion.ground_velocity_body.z + static_cast<calculation>(basis.forward[axis]) * motion.ground_velocity_body.x);
  }
  if(context.realistic && context.landed && speed < context.stall_metres_per_second) {
    motion.velocity_ned.z = 0;
    motion.acceleration_ned.z = 0;
    state.g_load = 1;
  }
  motion.angular_rate.x = static_cast<double>(static_cast<calculation>(motion.angular_acceleration.x) * seconds + motion.angular_rate.x);
  calculation const roll_inertia{static_cast<calculation>(inertia.fuel_kg) * inertia.fuel_kg * fuel_roll_inertia + inertia.roll_moment};
  calculation const roll_damping{static_cast<calculation>(context.density) * .5 * speed * p.wing_area * p.wing_span * p.wing_span * seconds / roll_inertia * p.roll_damping * .5};
  if(roll_damping < -1 && speed > minimum_damping_speed) {
    calculation const thrust{std::sin(engine_angle) * (static_cast<calculation>(context.engine_thrust) * context.thrust_moment_scale) * p.roll_thrust_arm * 2};
    calculation const dynamic{static_cast<calculation>(context.density) * speed * speed * p.wing_area * p.wing_span};
    motion.angular_acceleration.x = 0;
    motion.angular_rate.x = static_cast<double>((thrust / dynamic + static_cast<calculation>(context.roll_control_gradient) * p.maximum_aileron_radians) * ((-2 * speed / (static_cast<calculation>(p.wing_span) * p.roll_damping)) * context.aileron));
  }
  state.previous_pitch_rate = motion.angular_rate.y;
  motion.angular_rate.y = static_cast<double>(static_cast<calculation>(motion.angular_acceleration.y) * seconds + motion.angular_rate.y);
  calculation const pitch_damping{static_cast<calculation>(context.density) * .5 * speed * p.wing_area * p.mean_chord * p.mean_chord * seconds * (context.time_warp ? 5 : 1) / inertia.pitch_moment * p.pitch_damping * .5};
  if(pitch_damping < -1 && speed > minimum_damping_speed) {
    calculation const thrust{std::sin(engine_angle) * (static_cast<calculation>(context.engine_thrust) * context.thrust_moment_scale) * p.pitch_thrust_arm * 2};
    calculation const dynamic{static_cast<calculation>(context.density) * speed * speed * p.wing_area * p.mean_chord};
    calculation const control{(thrust / dynamic + static_cast<calculation>(context.pitch_control_gradient) * p.maximum_elevator_angle) * context.elevator + p.zero_pitch_moment};
    motion.angular_rate.y = static_cast<double>((-control / p.pitch_stability - context.angle_of_attack) / seconds);
    if(context.stick_pressure) motion.angular_rate.y = static_cast<double>(static_cast<calculation>(context.pitch_rate_bias) + motion.angular_rate.y);
    motion.angular_acceleration.y = 0;
  }
  double const positive_limit{native_pi * 10 * inverse_degrees_per_half_turn};
  if(motion.yaw_step_radians > positive_limit) motion.yaw_step_radians = positive_limit;
  double const negative_limit{native_pi * -10 * inverse_degrees_per_half_turn};
  if(motion.yaw_step_radians < negative_limit) motion.yaw_step_radians = negative_limit;
  state.low_speed_ground_contact = low_speed_ground;
}

} // namespace tfx::flight
