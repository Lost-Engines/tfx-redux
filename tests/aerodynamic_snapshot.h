#pragma once

#include <bit>
#include <utility>
#include <vector>
#include "flight/aerodynamic.h"

inline tfx::flight::aerodynamic_model aerodynamic_test_model(unsigned int const index) {
  std::array constexpr models{tfx::flight::aerodynamic_model::military_spec, tfx::flight::aerodynamic_model::realistic, tfx::flight::aerodynamic_model::arcade, tfx::flight::aerodynamic_model::simple};
  return models.at(index);
}

// Shared diagnostic encoding for initialization and connected flight traces.
// Every group follows named owners; native offsets are in the reference tool.
inline std::vector<uint64_t> aerodynamic_snapshot(tfx::flight::aerodynamic_flight const &s, tfx::flight::damage_state const &damage, tfx::math::random_state const &random) {
  std::vector<uint64_t> values;
  auto const number{[&](double const value){values.push_back(std::bit_cast<uint64_t>(value));}};
  auto const vector{[&](auto const &value){
    for(auto const component : value) number(component);
  }};
  auto const &m{s.response.motion};
  for(auto const *value : {&m.pose.metres_ned, &m.velocity_ned, &m.acceleration_ned, &m.ground_velocity_body, &m.air_velocity_body, &m.angular_rate}) vector(*value);
  vector(m.angular_acceleration);
  vector(m.rotation_remainder);
  number(m.yaw_step_radians);
  auto const &d{s.data};
  for(auto const value : {d.flight_path_pitch_degrees, d.altitude_km, d.angle_of_attack, d.metres_per_second, d.mach, d.dynamic_pressure, d.mass_kg, d.simulation_seconds, d.atmosphere.density, d.atmosphere.sea_level_kelvin, d.atmosphere.local_kelvin, d.atmosphere.sound_metres_per_second}) number(value);
  for(auto const *value : {&d.basis.forward, &d.basis.right, &d.basis.down}) vector(*value);
  auto const &c{s.coefficients};
  for(auto const value : {c.reference_drag, c.wave_drag, c.zero_lift_drag, c.lift_slope, c.pitch_stability, c.pitch_damping, c.elevator_moment, c.elevator_lift, c.roll_damping, c.roll_sideslip, c.aileron_effectiveness}) number(value);
  auto const &manual{s.manual};
  auto const &controls{manual.controls};
  for(auto const value : {controls.elevator, controls.aileron, controls.rudder, controls.keyboard_elevator, controls.keyboard_aileron, controls.keyboard_rudder, controls.minimum_elevator, controls.maximum_elevator, controls.aileron_limit, manual.elevator_trim, manual.pitch_rate_bias, manual.flap_fraction, manual.positive_g_lift_limit, manual.negative_g_lift_limit, manual.thrust_vector_fraction, manual.air_brake_fraction, controls.autopilot.down_metres, controls.autopilot.metres_per_second}) number(value);
  auto const &p{s.powerplant};
  for(auto const *value : {&p.engine_factor, &p.nozzle_angles}) vector(*value);
  vector(p.engine_force);
  vector(p.engine_moment);
  for(auto const value : {p.thrust, p.throttle_target, p.fuel, p.fuel_capacity, p.extra_fuel_capacity, p.fuel_flow}) number(value);
  auto const &surface{s.surfaces};
  for(auto const value : {surface.automatic_flap_fraction, surface.flap_lift_offset, surface.positive_lift_limit, surface.negative_lift_limit, surface.wing_lift_coefficient, surface.effective_angle, surface.lift_coefficient, surface.lift, surface.drag_coefficient, surface.drag, surface.pitch_rate_moment_term, surface.pitch_moment, surface.roll_rate_moment_term, surface.roll_moment}) number(value);
  vector(surface.force);
  vector(surface.moment);
  vector(s.forces.body_force);
  vector(s.forces.ned_force);
  vector(s.forces.body_moment);
  for(auto const value : {s.commands.roll_rate, s.commands.angle_of_attack, s.commands.body_lift_force, s.response.sideslip_radians, s.response.previous_pitch_rate, s.seconds, s.gear_fraction, s.coefficient_parameters.pitch_stability_correction, s.coefficient_parameters.pitch_damping_mach_gradient, s.stores.drag, s.stores.mass_kg, s.stores.moment_a, s.stores.moment_b, s.stores.total_moment_a, s.stores.total_moment_b, s.stores.fuel_capacity, damage.accumulated}) number(value);
  auto const &wind{s.wind};
  for(auto const value : {wind.simulation_seconds, wind.smooth_speed, wind.target_speed, wind.direction, wind.vertical_angle, wind.renewal_after, wind.direction_wobble_scale, wind.direction_phase_frequency, wind.direction_phase_offset, wind.direction_envelope_frequency, wind.vertical_phase_frequency, wind.vertical_phase_offset, wind.vertical_scale, wind.upper_wind.x, wind.upper_wind.y, wind.upper_mix, wind.legacy_gust_period, wind.legacy_gust_target, wind.legacy_gust_smoothing}) number(value);
  auto const &g{s.ground.ground};
  auto const &point{g.contact};
  for(auto const *value : {&point.offset_body, &point.point_ned, &point.velocity_body, &point.velocity_ned, &point.working_force, &point.force, &point.moment}) vector(*value);
  for(auto const value : {point.stiffness, point.damping, point.penetration, point.down_velocity, point.rolling_coefficient, point.braking_coefficient, point.resistance_force, point.unloaded_clearance, point.ground_up_metres}) number(value);
  for(auto const *value : {&g.hook_offset_ned, &g.hook_anchor_ned, &g.hook_displacement, &g.hook_force_ned, &g.hook_force_body, &g.hook_moment}) vector(*value);
  for(auto const value : {g.cable_length, g.cable_speed, g.rollout_distance}) number(value);
  vector(s.ground.wheel_penetration);
  for(auto const value : {s.ground.roll_angle, s.ground.pitch_angle, s.aircraft.ground.contact.maximum_sink_speed, s.aircraft.surfaces.wing_area, s.aircraft.surfaces.air_brake_drag}) number(value);
  for(auto const value : wind.velocity_ned) values.push_back(std::bit_cast<uint32_t>(value));
  values.push_back(std::bit_cast<uint32_t>(p.throttle));
  values.push_back(std::bit_cast<uint32_t>(s.response.g_load));
  values.push_back(static_cast<uint32_t>(d.fixed_speed));
  values.push_back(static_cast<uint32_t>(d.track_heading));
  for(unsigned int row{0}; row != 3; ++row) {
    for(auto const *value : {&m.pose.frame.forward, &m.pose.frame.right, &m.pose.frame.down}) values.push_back(static_cast<uint32_t>((*value)[row]));
  }
  for(auto const value : {static_cast<unsigned int>(controls.autopilot.enabled), static_cast<unsigned int>(controls.autopilot.mode), static_cast<unsigned int>(controls.autopilot.heading), static_cast<unsigned int>(p.afterburner), static_cast<unsigned int>(p.afterburner_control), static_cast<unsigned int>(p.afterburner_display), static_cast<unsigned int>(s.response.low_speed_ground_contact), static_cast<unsigned int>(g.landed), static_cast<unsigned int>(g.ground_steering), static_cast<unsigned int>(g.arrestor), static_cast<unsigned int>(g.last_contact_kind), static_cast<unsigned int>(s.arrestor_hook_down), static_cast<unsigned int>(s.chute_deployed), static_cast<unsigned int>(s.chute_cutaway), static_cast<unsigned int>(surface.stalled), static_cast<unsigned int>(static_cast<uint16_t>(m.collision)), static_cast<unsigned int>(s.stores.fuel_tank)}) values.push_back(value);
  for(auto const condition : damage.systems) values.push_back(std::to_underlying(condition));
  values.push_back(random.low);
  values.push_back(random.high);
  for(auto const value : {s.input_gains.pitch, s.input_gains.roll, s.input_gains.rudder, s.input_gains.joystick}) number(value);
  for(auto const value : s.view.position) values.push_back(std::bit_cast<uint32_t>(value));
  for(auto const value : s.view.integer_position) values.push_back(static_cast<uint32_t>(value));
  for(auto const value : s.view.angles) values.push_back(value);
  for(unsigned int row{0}; row != 3; ++row) {
    for(auto const *value : {&s.view.orientation.right, &s.view.orientation.down, &s.view.orientation.forward}) values.push_back(static_cast<uint32_t>((*value)[row]));
  }
  return values;
}
