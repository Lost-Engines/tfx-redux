#include "aerodynamic_contact.h"
#include <algorithm>
#include <bit>
#include <utility>
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr metres_per_foot{.3048}, retracted_wheel_down{.5};
double constexpr body_contact_stiffness{1.5}, sink_warning_factor{1.4}, sink_failure_factor{1.8};
double constexpr failed_gear_crash_speed{150}, failed_system_factor{.5};
double constexpr body_damage_sink_speed{2}, body_damage_forward_speed{40}, crash_sink_speed{10};
double constexpr minimum_forward_projection{.1}, lateral_friction{.4};
double constexpr dry_rolling_coefficient{.02}, rough_rolling_coefficient{.06};
double constexpr roughness_per_random_step{.00015259021896696422};
double constexpr minimum_right_projection{.01}, brake_fade_speed{30}, brake_fade_gradient{.004}, brake_fade_offset{.12};
double constexpr low_speed_brake_gradient{.1}, body_friction{.1};
int16_t constexpr minimum_downward_basis{2000};
int constexpr feet_per_ground_step{8};

calculation wide(double const value) { return value; }
double negative(double const value) {
  return std::bit_cast<double>(std::bit_cast<uint64_t>(value) ^ (uint64_t{1} << 63));
}
vec3d to_ned(aerodynamic_basis const &basis, vec3d const &body) {
  vec3d result;
  for(unsigned int axis{0}; axis != 3; ++axis)
    result[axis] = static_cast<double>(wide(basis.right[axis]) * body.y + wide(basis.forward[axis]) * body.x + wide(basis.down[axis]) * body.z);
  return result;
}

} // anonymous namespace

bool update_aerodynamic_contact(aerodynamic_contact_state &state, aerodynamic_motion &motion, damage_state &damage, math::random_state &random, aerodynamic_contact_parameters const &parameters, aerodynamic_contact_context const &context) {
  if(context.kind == aerodynamic_contact_kind::retracted_wheel) state.offset_body.z = retracted_wheel_down;
  auto const &offset{state.offset_body};
  auto const &basis{context.basis};
  state.point_ned = to_ned(basis,offset);
  for(unsigned int axis{0}; axis != 3; ++axis) state.point_ned[axis] = static_cast<double>(wide(motion.pose.metres_ned[axis]) + state.point_ned[axis]);
  state.ground_up_metres = context.terrain_present ? static_cast<double>(-context.ground_elevation_steps * feet_per_ground_step * wide(metres_per_foot)) : -8;
  double const ground_up{state.ground_up_metres};
  auto const &rates{motion.angular_rate};
  // This routine deliberately excludes yaw from omega cross r, then clears it.
  state.velocity_body.x = static_cast<double>(wide(rates.y) * offset.z - 0.0 * offset.y);
  state.velocity_body.y = static_cast<double>(0.0 * offset.x - wide(rates.x) * offset.z);
  state.velocity_body.z = static_cast<double>(wide(offset.y) * rates.x - wide(rates.y) * offset.x);
  for(unsigned int axis{0}; axis != 3; ++axis) state.velocity_body[axis] = static_cast<double>(wide(motion.ground_velocity_body[axis]) + state.velocity_body[axis]);
  state.velocity_ned = to_ned(basis,state.velocity_body);
  motion.angular_rate.z = 0;
  if(state.point_ned.z <= -ground_up || context.body_downward_q15 <= minimum_downward_basis) {
    state.unloaded_clearance = parameters.unloaded_clearance;
    return false;
  }
  state.stiffness = parameters.spring_stiffness;
  state.damping = parameters.damping;
  if(context.kind == aerodynamic_contact_kind::body) {
    state.stiffness = static_cast<double>(wide(state.stiffness) * body_contact_stiffness);
    state.damping = static_cast<double>(wide(state.damping) * body_contact_stiffness);
  }
  auto &gear{damage.systems[std::to_underlying(aircraft_system::landing_gear)]};
  auto &brakes{damage.systems[std::to_underlying(aircraft_system::wheel_brakes)]};
  bool impact{false};
  if(context.gear_damage_enabled && !context.low_speed_ground_contact) {
    if(context.kind == aerodynamic_contact_kind::wheel) {
      if(state.velocity_ned.z > wide(parameters.maximum_sink_speed) * sink_warning_factor) {
        if(gear != system_condition::failed) gear = system_condition::intermittent;
        if(state.velocity_ned.z > wide(parameters.maximum_sink_speed) * sink_failure_factor) gear = system_condition::failed;
      }
      if(state.velocity_body.x > parameters.rolling_warning_speed && gear != system_condition::failed) gear = system_condition::intermittent;
      if(state.velocity_body.x > parameters.rolling_failure_speed) gear = system_condition::failed;
      if(gear == system_condition::failed && state.velocity_body.x > failed_gear_crash_speed) motion.collision = 2;
      if(gear == system_condition::failed) {
        state.damping = static_cast<double>(wide(state.damping) * failed_system_factor);
        impact = true;
      }
    } else {
      if(state.velocity_ned.z > body_damage_sink_speed && state.velocity_body.x > body_damage_forward_speed) damage.accumulated = static_cast<double>(wide(damage.accumulated) + 1);
      if(state.velocity_ned.z > crash_sink_speed) motion.collision = 2;
      gear = system_condition::failed;
    }
    if(motion.collision || (gear == system_condition::failed && offset.y == parameters.left_main_wheel_right)) return impact;
  } else if(state.velocity_ned.z > crash_sink_speed) motion.collision = 2;

  state.penetration = static_cast<double>(wide(ground_up) + state.point_ned.z);
  state.down_velocity = state.velocity_ned.z;
  double const normal{static_cast<double>(-wide(state.penetration) * state.stiffness - wide(state.down_velocity) * state.damping)};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const vertical{axis == 0 ? basis.forward.z : axis == 1 ? basis.right.z : basis.down.z};
    state.working_force[axis] = static_cast<double>(wide(vertical) * normal);
    state.force[axis] = static_cast<double>(wide(state.working_force[axis]) + state.force[axis]);
  }
  state.moment.x = static_cast<double>(wide(offset.y) * state.working_force.z - wide(offset.z) * state.working_force.y + state.moment.x);
  state.moment.y = static_cast<double>(wide(offset.z) * state.working_force.x - wide(offset.x) * state.working_force.z + state.moment.y);
  state.moment.z = static_cast<double>(wide(offset.x) * state.working_force.y - wide(offset.y) * state.working_force.x + state.moment.z);

  double const forward_length{static_cast<double>(std::sqrt(static_cast<calculation>(static_cast<double>(wide(basis.forward.y) * basis.forward.y + wide(basis.forward.x) * basis.forward.x))))};
  if(forward_length > minimum_forward_projection) {
    calculation const inverse_length{1.0 / forward_length};
    calculation const sideways_force{(wide(basis.forward.x) * state.velocity_ned.y + -wide(basis.forward.y) * state.velocity_ned.x) * inverse_length * lateral_friction * normal};
    state.working_force.y = static_cast<double>((-wide(basis.forward.y) * basis.right.x + wide(basis.forward.x) * basis.right.y) * sideways_force * inverse_length);
    state.working_force.z = static_cast<double>((-wide(basis.forward.y) * basis.down.x + wide(basis.forward.x) * basis.down.y) * sideways_force * inverse_length);
    state.force.y = static_cast<double>(wide(state.working_force.y) + state.force.y);
    state.force.z = static_cast<double>(wide(state.working_force.z) + state.force.z);
    // Original lateral friction applies only roll torque, with two stores.
    state.moment.x = static_cast<double>(wide(state.moment.x) - wide(offset.z) * state.working_force.y);
    state.moment.x = static_cast<double>(wide(offset.y) * state.working_force.z + state.moment.x);
  }
  state.rolling_coefficient = context.precipitation ? parameters.wet_rolling_coefficient : dry_rolling_coefficient;
  if(context.rough_surface) {
    state.rolling_coefficient = rough_rolling_coefficient;
    calculation const bump{wide(static_cast<double>(random.next())) * roughness_per_random_step * state.velocity_body.x};
    state.force.z = static_cast<double>(wide(state.force.z) - bump);
    state.moment.x = static_cast<double>(wide(state.moment.x) - -bump * offset.y);
    state.moment.y = static_cast<double>(-bump * offset.x + state.moment.y);
  }
  double const right_length{static_cast<double>(std::sqrt(static_cast<calculation>(static_cast<double>(wide(basis.right.y) * basis.right.y + wide(basis.right.x) * basis.right.x))))};
  if(right_length <= minimum_right_projection) return impact;
  state.resistance_force = static_cast<double>(wide(state.rolling_coefficient) * normal);
  if(context.wheel_brakes && context.kind == aerodynamic_contact_kind::wheel) {
    auto const &velocity{motion.velocity_ned};
    double const speed{static_cast<double>(std::sqrt(wide(velocity.y) * velocity.y + wide(velocity.x) * velocity.x + wide(velocity.z) * velocity.z))};
    state.braking_coefficient = parameters.braking_coefficient;
    if(brakes == system_condition::intermittent) state.braking_coefficient = static_cast<double>(wide(state.braking_coefficient) * failed_system_factor);
    if(speed > brake_fade_speed && brakes != system_condition::failed) state.braking_coefficient = static_cast<double>(brake_fade_offset - wide(speed) * brake_fade_gradient + state.braking_coefficient);
    if(state.braking_coefficient < 0) state.braking_coefficient = 0;
    if(speed < crash_sink_speed) state.braking_coefficient = static_cast<double>(wide(parameters.braking_coefficient) * speed * low_speed_brake_gradient);
    state.braking_coefficient = std::max(state.braking_coefficient,state.rolling_coefficient);
    if(context.precipitation) state.braking_coefficient = static_cast<double>(wide(parameters.wet_braking_factor) * state.braking_coefficient);
    if(brakes == system_condition::failed) state.braking_coefficient = 0;
    calculation const braking{wide(parameters.empty_mass_kg) / context.total_mass_kg * state.braking_coefficient};
    state.braking_coefficient = static_cast<double>(braking);
    state.resistance_force = static_cast<double>(braking * normal);
    // Brakes can fail now; this point's resistance used their preceding state.
    if(speed > parameters.brake_warning_speed && brakes != system_condition::failed) brakes = system_condition::intermittent;
    if(speed > parameters.brake_failure_speed) brakes = system_condition::failed;
  }
  if(context.kind == aerodynamic_contact_kind::body) state.resistance_force = static_cast<double>(wide(normal) * body_friction);
  if(wide(state.velocity_body.x) * state.resistance_force > 0) state.resistance_force = negative(state.resistance_force);
  calculation const inverse_right_length{1.0 / right_length};
  state.working_force.x = static_cast<double>((wide(basis.forward.x) * basis.right.y - wide(basis.forward.y) * basis.right.x) * state.resistance_force * inverse_right_length);
  state.working_force.z = static_cast<double>((wide(basis.down.x) * basis.right.y - wide(basis.down.y) * basis.right.x) * state.resistance_force * inverse_right_length);
  state.force.x = static_cast<double>(wide(state.working_force.x) + state.force.x);
  state.force.z = static_cast<double>(wide(state.working_force.z) + state.force.z);
  // Original longitudinal friction changes only pitch torque.
  state.moment.y = static_cast<double>(wide(offset.z) * state.working_force.x + state.moment.y);
  state.moment.y = static_cast<double>(wide(state.moment.y) - wide(offset.x) * state.working_force.z);
  return impact;
}

} // namespace tfx::flight
