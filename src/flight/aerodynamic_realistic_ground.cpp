#include "aerodynamic_realistic_ground.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>
#include "flight/aerodynamic_ground_internal.h"

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr gravity{9.81}, metres_per_foot{.3048}, retracted_wheel_down{.5};
double constexpr gear_failure_multiplier{1.2}, rolling_crash_speed{150}, disabled_damage_crash_sink{20}, sink_failure_multiplier{1.4};
double constexpr launch_throttle{.6}, launch_force{700000}, dry_rolling{.02}, rough_rolling{.06};
double constexpr roughness_per_random_step{.0015259021896696422};
double constexpr minimum_right_projection{.01}, intermittent_brake_factor{.5}, brake_fade_speed{30}, brake_fade_gradient{.006}, brake_fade_offset{.18};
double constexpr low_brake_speed{10}, low_brake_gradient{.1}, body_friction{.1};
double constexpr roll_settling_rate{.125}, pitch_settling_rate{.25}, main_wheel_average{.5};
double constexpr velocity_settling_divisor{4}, height_settling_rate{.2};
int constexpr feet_per_ground_step{8};

calculation wide(double const value) { return value; }
double negative(double const value) {
  return std::bit_cast<double>(std::bit_cast<uint64_t>(value) ^ (uint64_t{1} << 63));
}
calculation native_asin(double const value) {
  calculation const x{value};
  return std::atan2(x,std::sqrt(1.0 - x * x));
}
void project(aerodynamic_contact_state &state, aerodynamic_motion const &motion, aerodynamic_contact_context const &context, aerodynamic_contact_kind const kind) {
  if(kind == aerodynamic_contact_kind::retracted_wheel) state.offset_body.z = retracted_wheel_down;
  auto const &basis{context.basis};
  auto const &offset{state.offset_body};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    state.point_ned[axis] = static_cast<double>(wide(basis.right[axis]) * offset.y + wide(basis.forward[axis]) * offset.x + wide(basis.down[axis]) * offset.z);
    state.point_ned[axis] = static_cast<double>(wide(motion.pose.metres_ned[axis]) + state.point_ned[axis]);
  }
  state.ground_up_metres = context.terrain_present ? static_cast<double>(-context.ground_elevation_steps * feet_per_ground_step * wide(metres_per_foot)) : -8;
}

} // anonymous namespace

aerodynamic_ground_events update_aerodynamic_realistic_ground(aerodynamic_realistic_ground_state &state, aerodynamic_motion &motion, aerodynamic_control_state &controls, damage_state &damage, math::random_state &random, aerodynamic_ground_parameters const &parameters, aerodynamic_realistic_ground_context const &context) {
  aerodynamic_ground_events events;
  auto &ground{state.ground};
  auto &point{ground.contact};
  auto const &input{context.ground};
  auto const &basis{input.contact.basis};
  point.force = vec3d{0,0,0};
  point.moment = vec3d{0,0,0};
  if(!input.contact.terrain_present) {
    ground.landed = ground.ground_steering = false;
    return events;
  }
  if(ground.landed && (input.metres_per_second < context.stall_metres_per_second || controls.keyboard_elevator > 0)) controls.keyboard_elevator = 0;
  auto const &velocity{motion.velocity_ned};
  double const speed{static_cast<double>(std::sqrt(wide(velocity.y) * velocity.y + wide(velocity.x) * velocity.x + wide(velocity.z) * velocity.z))};
  ground.last_contact_kind = aerodynamic_contact_kind::retracted_wheel;
  auto &gear{damage.systems[std::to_underlying(aircraft_system::landing_gear)]};
  auto &brakes{damage.systems[std::to_underlying(aircraft_system::wheel_brakes)]};
  if(input.gear_fraction != 0 && input.contact.gear_damage_enabled) {
    ground.last_contact_kind = aerodynamic_contact_kind::wheel;
    if(input.metres_per_second > parameters.maximum_gear_speed && gear != system_condition::failed) gear = system_condition::intermittent;
    if(input.metres_per_second > wide(parameters.maximum_gear_speed) * gear_failure_multiplier) gear = system_condition::failed;
  }
  for(unsigned int wheel{0}; wheel != 3; ++wheel) {
    point.offset_body = parameters.offsets[wheel];
    project(point,motion,input.contact,ground.last_contact_kind);
    state.wheel_penetration[wheel] = static_cast<double>(wide(point.point_ned.z) + point.ground_up_metres);
  }
  state.roll_angle = static_cast<double>(native_asin(basis.right.z));
  state.pitch_angle = static_cast<double>(-native_asin(basis.forward.z));
  auto const &penetration{state.wheel_penetration};
  if(penetration.x < 0 && penetration.y < 0 && penetration.z < 0 && (!ground.landed || (-wide(input.contact.total_mass_kg) * gravity > input.previous_ned_force.z && input.metres_per_second > context.stall_metres_per_second))) {
    ground.landed = ground.ground_steering = false;
    return events;
  }
  if(ground.ground_steering && input.engine_factor.x != 0 && input.engine_factor.y != 0 && input.throttle >= launch_throttle) {
    point.force.x = static_cast<double>(wide(basis.forward.x) * launch_force + point.force.x);
    point.force.z = static_cast<double>(wide(basis.down.x) * launch_force + point.force.z);
  }
  if(input.contact.gear_damage_enabled) {
    if(speed > parameters.contact.rolling_warning_speed && gear != system_condition::failed) gear = system_condition::intermittent;
    if(speed > parameters.contact.rolling_failure_speed) gear = system_condition::failed;
    if(gear == system_condition::failed && speed > rolling_crash_speed) motion.collision = 1;
    if(gear == system_condition::failed) ++events.gear_impacts;
  } else if(motion.velocity_ned.z > disabled_damage_crash_sink) motion.collision = 1;
  if(!ground.landed && input.contact.gear_damage_enabled) {
    if(motion.velocity_ned.z > parameters.contact.maximum_sink_speed && gear != system_condition::failed) gear = system_condition::intermittent;
    if(motion.velocity_ned.z > wide(parameters.contact.maximum_sink_speed) * sink_failure_multiplier) gear = system_condition::failed;
  }
  calculation const support{-wide(input.contact.total_mass_kg) * gravity};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const vertical{axis == 0 ? basis.forward.z : axis == 1 ? basis.right.z : basis.down.z};
    point.working_force[axis] = static_cast<double>(wide(vertical) * support);
    point.force[axis] = static_cast<double>(wide(point.working_force[axis]) + point.force[axis]);
  }
  ground.landed = true;
  point.rolling_coefficient = dry_rolling;
  if(input.contact.rough_surface && !input.carrier_surface) {
    point.rolling_coefficient = rough_rolling;
    point.force.z = static_cast<double>(wide(point.force.z) - wide(static_cast<double>(random.next())) * roughness_per_random_step * motion.air_velocity_body.x);
  }
  double const squared_right{static_cast<double>(wide(basis.right.y) * basis.right.y + wide(basis.right.x) * basis.right.x)};
  ground.cable_speed = static_cast<double>(std::sqrt(wide(squared_right)));
  if(ground.cable_speed > minimum_right_projection) {
    double resistance{static_cast<double>((wide(input.contact.total_mass_kg) * gravity - context.lift) * point.rolling_coefficient)};
    if(input.contact.wheel_brakes && input.gear_fraction != 0) {
      point.braking_coefficient = parameters.contact.braking_coefficient;
      if(brakes == system_condition::intermittent) point.braking_coefficient = static_cast<double>(wide(point.braking_coefficient) * intermittent_brake_factor);
      if(speed > brake_fade_speed && brakes != system_condition::failed) point.braking_coefficient = static_cast<double>(brake_fade_offset - wide(speed) * brake_fade_gradient + point.braking_coefficient);
      if(point.braking_coefficient < 0) point.braking_coefficient = 0;
      if(speed < low_brake_speed) point.braking_coefficient = static_cast<double>(wide(parameters.contact.braking_coefficient) * speed * low_brake_gradient);
      point.braking_coefficient = std::max(point.braking_coefficient,point.rolling_coefficient);
      if(input.contact.precipitation) point.braking_coefficient = static_cast<double>(wide(parameters.contact.wet_braking_factor) * point.braking_coefficient);
      if(brakes == system_condition::failed) point.braking_coefficient = 0;
      resistance = static_cast<double>((wide(input.contact.total_mass_kg) * gravity - context.lift) * point.braking_coefficient);
      if(speed > parameters.contact.brake_warning_speed && brakes != system_condition::failed) brakes = system_condition::intermittent;
      if(speed > parameters.contact.brake_failure_speed) brakes = system_condition::failed;
    }
    if(input.gear_fraction == 0) resistance = static_cast<double>((wide(input.contact.total_mass_kg) * gravity - context.lift) * body_friction);
    if(motion.air_velocity_body.x > 0) resistance = negative(resistance);
    calculation const inverse_length{1.0 / ground.cable_speed};
    point.working_force.x = static_cast<double>((wide(basis.forward.x) * basis.right.y - wide(basis.forward.y) * basis.right.x) * resistance * inverse_length);
    point.working_force.z = static_cast<double>((wide(basis.down.x) * basis.right.y - wide(basis.down.y) * basis.right.x) * resistance * inverse_length);
    point.force.x = static_cast<double>(wide(point.working_force.x) + point.force.x);
    point.force.z = static_cast<double>(wide(point.working_force.z) + point.force.z);
  }
  if(penetration.x < 0 && ((penetration.y > 0 && penetration.z < 0) || (penetration.y < 0 && penetration.z > 0))) {
    auto const &wheel{parameters.offsets[penetration.y > 0 ? 1u : 2u]};
    ground.cable_speed = static_cast<double>(std::cos(state.roll_angle) * wheel.y - std::sin(state.roll_angle) * wheel.z);
    state.commanded_roll_rate = static_cast<double>(-wide(state.roll_angle) * roll_settling_rate);
    state.commanded_angle_of_attack = static_cast<double>((wide(context.angle_of_attack) - state.pitch_angle) * pitch_settling_rate + state.commanded_angle_of_attack);
  }
  if((penetration.x < 0 && penetration.y > 0 && penetration.z > 0) || (penetration.x > 0 && penetration.y < 0 && penetration.z < 0)) {
    state.commanded_roll_rate = static_cast<double>(-wide(state.roll_angle) * roll_settling_rate);
    auto const &wheel{parameters.offsets[2]};
    ground.cable_speed = static_cast<double>(std::cos(state.pitch_angle) * -wide(wheel.x) - std::sin(state.pitch_angle) * wheel.z);
    // The wheel-height expression is retained, but native correction uses
    // the pitch angle still on the FPU stack, rather than that stored height.
    state.commanded_angle_of_attack = static_cast<double>(-wide(state.pitch_angle) * pitch_settling_rate + state.commanded_angle_of_attack);
    if(controls.keyboard_elevator > 0) state.commanded_angle_of_attack = 0;
    if(input.metres_per_second < context.stall_metres_per_second) motion.pose.metres_ned.z = static_cast<double>(wide(motion.pose.metres_ned.z) - (wide(penetration.y) + penetration.z) * main_wheel_average * pitch_settling_rate);
    else motion.velocity_ned.z = static_cast<double>(wide(motion.velocity_ned.z) - (wide(penetration.y) + penetration.z) * main_wheel_average / (wide(input.seconds) * velocity_settling_divisor));
  }
  if(penetration.x > 0 && penetration.y > 0 && penetration.z > 0) {
    state.commanded_roll_rate = static_cast<double>(-wide(state.roll_angle) * roll_settling_rate);
    if(state.commanded_angle_of_attack < context.angle_of_attack) state.commanded_angle_of_attack = static_cast<double>(wide(context.angle_of_attack) - state.pitch_angle);
    state.commanded_angle_of_attack = static_cast<double>(-wide(state.pitch_angle) * pitch_settling_rate + state.commanded_angle_of_attack);
    if(controls.keyboard_elevator > 0) state.commanded_angle_of_attack = 0;
    calculation const down{wide(input.contact.ground_elevation_steps) * metres_per_foot * feet_per_ground_step - parameters.offsets[0].z + wide(input.contact.total_mass_kg) * gravity / parameters.contact.spring_stiffness};
    motion.pose.metres_ned.z = static_cast<double>((down - motion.pose.metres_ned.z) * height_settling_rate + motion.pose.metres_ned.z);
  }
  update_aerodynamic_arrestor(ground,motion,parameters,input,false);
  return events;
}

} // namespace tfx::flight
