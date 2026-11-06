#include "aerodynamic_guidance.h"
#include <cmath>
#include <numbers>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr native_pi{3.14159}, inverse_angle_half_turn{3.0517578125e-05}, inverse_degrees{.005555555555555555};
double constexpr gravity{9.81}, climb_error_scale{.001}, minimum_target_down{-19812}, maximum_target_down{-304.8};
double constexpr vertical_deadband{.5}, bank_margin{.95}, heading_bank_scale{2}, followed_bank_scale{3};
double constexpr bank_limit_degrees{80}, clamped_bank_degrees{83}, nighthawk_bank_scale{.6};
double constexpr pitch_cosine_limit{.01}, minimum_response_speed{10}, pitch_bias_bank_threshold{.1}, pitch_error_scale{2.5};
double constexpr recovery_descent_scale{5}, recovery_pitch_multiplier{3};
double constexpr pitch_rate_feedback{1.2}, vertical_acceleration_feedback{24}, realistic_feedback_seconds{.06}, secondary_vertical_feedback{48};
double constexpr roll_error_gain{1.3}, maximum_roll_step_degrees{6}, fuel_roll_inertia{.004166666666666667};
double constexpr angle_units_per_half_turn{32768}, pitch_snap_fraction{.16666666666666666};
double constexpr recovery_speed_threshold{200}, recovery_target_speed{1000};
double constexpr settled_bank{.01}, settled_roll_rate{.02}, settled_flight_path{.02}, settled_pitch_rate{.05};
uint16_t constexpr followed_mode{2}, recovery_mode{4};
double constexpr positive_bank_threshold{native_pi * bank_limit_degrees * inverse_degrees};
double constexpr negative_bank_threshold{native_pi * -bank_limit_degrees * inverse_degrees};
double constexpr positive_clamped_bank{native_pi * clamped_bank_degrees * inverse_degrees};
double constexpr negative_clamped_bank{native_pi * -clamped_bank_degrees * inverse_degrees};
double constexpr positive_roll_step{native_pi * maximum_roll_step_degrees * inverse_degrees};
double constexpr negative_roll_step{native_pi * -maximum_roll_step_degrees * inverse_degrees};

calculation wide(double const value) { return value; }
calculation native_asin(calculation const x) { return std::atan2(x,std::sqrt(1.0 - x * x)); }
calculation native_acos(calculation const x) {
  // Native 0xd254e subtracts asin from the actual extended half-pi constant.
  return std::numbers::pi_v<calculation> / 2 - native_asin(x);
}
void wrap_radians(double &value) {
  if(value > native_pi) value = static_cast<double>(wide(value) - wide(native_pi) * 2);
  if(value < -native_pi) value = static_cast<double>(wide(native_pi) * 2 + value);
}
void control_speed(aerodynamic_guidance_events &events, aerodynamic_manual_state &manual, aerodynamic_powerplant &powerplant, aerodynamic_guidance_context const &context, bool const recover) {
  aerodynamic_speed_state speed{
    .target_metres_per_second{manual.controls.autopilot.metres_per_second},
    .air_brake_fraction{manual.air_brake_fraction},
    .air_brake_condition{manual.air_brake_condition},
  };
  aerodynamic_speed_context const input{
    .metres_per_second{context.law.data.metres_per_second},
    .mass_kg{context.law.data.mass_kg},
    .forward_force{context.law.body_forward_force},
    .air_brake_drag{context.law.geometry.air_brake_drag},
    .afterburner_stages{context.engine.afterburner_stages},
    .strategy{recover ? aerodynamic_speed_strategy::recover : aerodynamic_speed_strategy::maintain},
  };
  auto const change{update_aerodynamic_speed(speed,powerplant,input)};
  manual.controls.autopilot.metres_per_second = speed.target_metres_per_second;
  manual.air_brake_fraction = speed.air_brake_fraction;
  manual.air_brake_condition = speed.air_brake_condition;
  if(change != afterburner_change::none) events.afterburner.push_back(change);
}

} // anonymous namespace

aerodynamic_guidance_events update_aerodynamic_guidance(aerodynamic_guidance &state, aerodynamic_manual_state &manual, aerodynamic_powerplant &powerplant, aerodynamic_motion &motion, aerodynamic_recovery_state &recovery, aerodynamic_guidance_context const &context) {
  aerodynamic_guidance_events events;
  auto &targets{manual.controls.autopilot};
  auto const &law{context.law};
  auto const &data{law.data};
  auto const &basis{data.basis};
  auto const &p{law.geometry};
  auto const &c{law.coefficients};
  state.saved_roll_rate = motion.angular_rate.x;
  int32_t const phase{-static_cast<int32_t>(math::signed_word(static_cast<uint16_t>(targets.heading - static_cast<uint16_t>(data.track_heading))))};
  state.heading_error = static_cast<double>(wide(native_pi) * phase * inverse_angle_half_turn);
  state.ground_speed = static_cast<double>(std::sqrt(wide(motion.velocity_ned.y) * motion.velocity_ned.y + wide(motion.velocity_ned.x) * motion.velocity_ned.x + wide(motion.velocity_ned.z) * motion.velocity_ned.z));
  state.climb_speed_limit = static_cast<double>((wide(context.engine.dry_thrust) + context.engine.afterburner_thrust - context.previous_drag) * state.ground_speed / (wide(data.mass_kg) * gravity));
  wrap_radians(state.heading_error);
  if(targets.down_metres > maximum_target_down) targets.down_metres = maximum_target_down;
  if(targets.down_metres < minimum_target_down) targets.down_metres = minimum_target_down;
  state.requested_vertical_speed = static_cast<double>((wide(targets.down_metres) - motion.pose.metres_ned.z) * climb_error_scale * state.climb_speed_limit);
  if(state.requested_vertical_speed < vertical_deadband && state.requested_vertical_speed > -vertical_deadband) state.requested_vertical_speed = 0;
  if(state.requested_vertical_speed < -wide(state.climb_speed_limit)) state.requested_vertical_speed = -state.climb_speed_limit;
  if(state.requested_vertical_speed > state.climb_speed_limit) state.requested_vertical_speed = state.climb_speed_limit;
  if(state.requested_vertical_speed < targets.metres_per_second && state.requested_vertical_speed > -targets.metres_per_second && targets.mode != recovery_mode && targets.mode != followed_mode) state.desired_flight_path = static_cast<double>(-native_asin(wide(state.requested_vertical_speed) / targets.metres_per_second));
  if(motion.velocity_ned.z < state.ground_speed && motion.velocity_ned.z > -state.ground_speed) state.actual_flight_path = static_cast<double>(native_asin(-wide(motion.velocity_ned.z) / state.ground_speed));
  calculation const dynamic{wide(data.metres_per_second) * data.atmosphere.density * data.metres_per_second * p.wing_area};
  double const stored_dynamic{static_cast<double>(dynamic)};
  double const weight{static_cast<double>(wide(data.mass_kg) * 2 * gravity)};
  state.maximum_bank = static_cast<double>(native_acos(wide(weight) / (dynamic * law.surfaces.positive_lift_limit)) * bank_margin);
  if(manual.positive_g_lift_limit < law.surfaces.positive_lift_limit) state.maximum_bank = static_cast<double>(native_acos(wide(weight) / (wide(stored_dynamic) * manual.positive_g_lift_limit)) * bank_margin);
  // Original threshold and assigned clamp differ (80 versus 83 degrees).
  if(state.maximum_bank > positive_bank_threshold) state.maximum_bank = positive_clamped_bank;
  else if(state.maximum_bank < negative_bank_threshold) state.maximum_bank = negative_clamped_bank;
  if(law.aircraft == player_aircraft::nighthawk) state.maximum_bank = static_cast<double>(wide(state.maximum_bank) * nighthawk_bank_scale);
  state.desired_bank = static_cast<double>(std::sin(state.heading_error) * heading_bank_scale * state.maximum_bank);
  if(targets.mode == followed_mode && context.followed_aircraft) state.desired_bank = static_cast<double>(wide(state.desired_bank) * followed_bank_scale);
  if(state.desired_bank > state.maximum_bank) state.desired_bank = state.maximum_bank;
  if(state.desired_bank < -wide(state.maximum_bank)) state.desired_bank = -state.maximum_bank;
  auto const raw_pitch{native_asin(-wide(basis.forward.z))};
  double const pitch{static_cast<double>(raw_pitch)};
  auto const cosine{std::cos(raw_pitch)};
  double const stored_cosine{static_cast<double>(cosine)};
  if(cosine > pitch_cosine_limit || stored_cosine < -pitch_cosine_limit) {
    double const divisor{static_cast<double>(std::cos(pitch))};
    double const ratio{static_cast<double>(wide(basis.right.z) / divisor)};
    state.roll_angle = static_cast<double>(-native_asin(ratio >= -1 && ratio <= 1 ? wide(ratio) : wide(basis.right.z)));
  } else state.roll_angle = 0;
  if(basis.down.z >= 0 && data.metres_per_second > minimum_response_speed) {
    manual.pitch_rate_bias = basis.down.z > pitch_bias_bank_threshold ? static_cast<double>(std::sin(state.roll_angle) * gravity * std::tan(state.roll_angle) / data.metres_per_second) : 0;
    if(manual.pitch_rate_bias < 0) manual.pitch_rate_bias = -manual.pitch_rate_bias;
    manual.pitch_rate_bias = static_cast<double>((wide(state.desired_flight_path) - state.actual_flight_path) * pitch_error_scale + manual.pitch_rate_bias);
    if(targets.mode == recovery_mode && state.actual_flight_path < 0) manual.pitch_rate_bias = static_cast<double>((-wide(state.actual_flight_path) * recovery_descent_scale + recovery_pitch_multiplier) * manual.pitch_rate_bias);
    calculation const pitch_damping{wide(data.atmosphere.density) * .5 * data.metres_per_second * p.wing_area * p.mean_chord * p.mean_chord * context.seconds / context.pitch_moment * c.pitch_damping * .5};
    if(pitch_damping >= -.5 || !law.military) {
      if(law.military) manual.pitch_rate_bias = static_cast<double>(wide(manual.pitch_rate_bias) - wide(basis.down.z) * basis.down.z * motion.angular_rate.y * pitch_rate_feedback / (context.time_warp ? 5 : 1));
      calculation const acceleration{wide(motion.acceleration_ned.z) / data.metres_per_second * (wide(basis.down.z) * basis.down.z) * vertical_acceleration_feedback};
      manual.pitch_rate_bias = static_cast<double>(acceleration * (law.military ? context.seconds : realistic_feedback_seconds) + manual.pitch_rate_bias);
      if(law.military) manual.pitch_rate_bias = static_cast<double>(wide(motion.acceleration_ned.z) / data.metres_per_second * (wide(basis.down.z) * basis.down.z) * secondary_vertical_feedback * context.seconds / (context.time_warp ? 2 : 1) + manual.pitch_rate_bias);
    }
  }
  state.yaw_demand = 0;
  if(targets.mode == recovery_mode) state.desired_bank = 0;
  state.roll_error = static_cast<double>(wide(state.desired_bank) - state.roll_angle);
  wrap_radians(state.roll_error);
  state.roll_control_rate = static_cast<double>(wide(data.metres_per_second) / (wide(p.wing_span) * c.roll_damping) * (wide(c.aileron_effectiveness) * p.maximum_aileron_angle));
  state.commanded_roll_rate = static_cast<double>(wide(state.roll_error) * roll_error_gain / native_pi * state.roll_control_rate);
  if(targets.mode == recovery_mode) state.commanded_roll_rate = static_cast<double>((wide(basis.right.z) * 2 * basis.right.z + 1 + (1.0 - basis.down.z) * 4) * state.commanded_roll_rate);
  double const seconds{context.seconds};
  if(wide(state.commanded_roll_rate) * seconds > positive_roll_step) state.commanded_roll_rate = static_cast<double>(wide(positive_roll_step) / seconds);
  if(wide(state.commanded_roll_rate) * seconds < negative_roll_step) state.commanded_roll_rate = static_cast<double>(wide(negative_roll_step) / seconds);
  calculation const roll_damping{wide(data.atmosphere.density) * .5 * data.metres_per_second * p.wing_area * p.wing_span * p.wing_span * seconds / (wide(powerplant.fuel) * powerplant.fuel * fuel_roll_inertia + context.roll_moment) * c.roll_damping * .5};
  if((!law.military || (roll_damping >= -1 && data.metres_per_second > minimum_response_speed)) && wide(state.commanded_roll_rate) * seconds * state.commanded_roll_rate * seconds > wide(state.roll_error) * state.roll_error) {
    state.commanded_roll_rate = 0;
    motion.angular_rate.x = 0;
    rotate_aerodynamic_units(motion,aerodynamic_rotation::roll,static_cast<double>(wide(state.roll_error) * angle_units_per_half_turn / native_pi));
  }
  calculation const pitch_damping{wide(data.atmosphere.density) * .5 * data.metres_per_second * p.wing_area * p.mean_chord * p.mean_chord * seconds / context.pitch_moment * c.pitch_damping * .5};
  double const pitch_error{static_cast<double>(wide(state.desired_flight_path) - state.actual_flight_path)};
  if((!law.military || (pitch_damping >= -.5 && data.metres_per_second > minimum_response_speed)) && wide(manual.pitch_rate_bias) * seconds * manual.pitch_rate_bias * seconds > wide(pitch_error) * pitch_error) {
    manual.pitch_rate_bias = 0;
    motion.angular_rate.y = 0;
    rotate_aerodynamic_units(motion,aerodynamic_rotation::pitch,static_cast<double>(wide(pitch_error) * pitch_snap_fraction * angle_units_per_half_turn / native_pi));
  }
  if(targets.mode == recovery_mode) {
    if(data.metres_per_second < recovery_speed_threshold) {
      double const target{targets.metres_per_second};
      targets.metres_per_second = recovery_target_speed;
      control_speed(events,manual,powerplant,context,true);
      targets.metres_per_second = target;
    }
    if(state.roll_angle < settled_bank && state.roll_angle > -settled_bank && state.saved_roll_rate < settled_roll_rate && state.saved_roll_rate > -settled_roll_rate && state.actual_flight_path < settled_flight_path && state.actual_flight_path > -settled_flight_path && motion.angular_rate.y < settled_pitch_rate && motion.angular_rate.y > -settled_pitch_rate) {
      auto const result{toggle_aerodynamic_recovery(recovery,manual.controls,powerplant,manual.air_brake_condition,context.recovery)};
      events.recovery = result.change;
      events.afterburner.insert(events.afterburner.end(),result.afterburner.begin(),result.afterburner.end());
    }
  } else {
    if(data.metres_per_second < targets.metres_per_second) control_speed(events,manual,powerplant,context,true);
    if(data.metres_per_second > targets.metres_per_second) control_speed(events,manual,powerplant,context,false);
  }
  return events;
}

void apply_aerodynamic_guidance(aerodynamic_guidance const &state, aerodynamic_manual_state &manual, aerodynamic_powerplant const &powerplant, aerodynamic_guidance_context const &context) {
  uint16_t constexpr external_control_mode{3};
  double constexpr maximum_vector_angle{native_pi * 20 * inverse_degrees};
  if(manual.controls.autopilot.mode != external_control_mode) {
    auto const &law{context.law};
    auto const &data{law.data};
    auto const &p{law.geometry};
    auto const &c{law.coefficients};
    auto &controls{manual.controls};
    calculation const vector{(wide(powerplant.thrust) * manual.thrust_vector_fraction) * std::sin(maximum_vector_angle)};
    calculation const roll_thrust{vector * context.engine.roll_thrust_arm * 2};
    calculation const dynamic{wide(data.atmosphere.density) * data.metres_per_second * data.metres_per_second * p.wing_area};
    calculation const inverse_speed{1.0 / (wide(data.metres_per_second) * 2)};
    calculation const roll_control{wide(c.aileron_effectiveness) * p.maximum_aileron_angle + roll_thrust / (dynamic * p.wing_span)};
    controls.aileron = static_cast<double>((-wide(state.commanded_roll_rate) * c.roll_damping * p.wing_span * inverse_speed) / roll_control);
    double const pitch_thrust{static_cast<double>(vector * context.engine.pitch_thrust_arm * 2)};
    double const pitch_dynamic{static_cast<double>(dynamic * p.mean_chord)};
    double const inverse_dynamic{static_cast<double>(1.0 / pitch_dynamic)};
    double const pitch_contribution{static_cast<double>(wide(pitch_thrust) * inverse_dynamic)};
    calculation const elevator_control{wide(c.elevator_moment) * p.maximum_elevator_angle};
    double const stored_elevator_control{static_cast<double>(elevator_control)};
    double const denominator{static_cast<double>(elevator_control + pitch_contribution)};
    double const inverse_denominator{static_cast<double>(1.0 / denominator)};
    controls.elevator = static_cast<double>(-(wide(manual.pitch_rate_bias) * c.pitch_damping * p.mean_chord * inverse_speed) * inverse_denominator);
    controls.rudder = static_cast<double>(wide(state.yaw_demand) / context.rudder_effectiveness);
    controls.keyboard_elevator = controls.elevator;
    controls.keyboard_aileron = controls.aileron;
    controls.keyboard_rudder = controls.rudder;
    if(law.aircraft == player_aircraft::raptor) manual.elevator_trim = static_cast<double>(-(wide(data.angle_of_attack) * c.pitch_stability + p.zero_pitch_moment) * inverse_denominator);
    else manual.elevator_trim = static_cast<double>((wide(data.angle_of_attack) * c.pitch_stability + p.zero_pitch_moment) * (-1.0 / stored_elevator_control));
  }
  manual.pitch_rate_bias = 0;
}

} // namespace tfx::flight
