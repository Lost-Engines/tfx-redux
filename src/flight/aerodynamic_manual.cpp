#include "aerodynamic_control_law_internal.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr native_pi{3.14159}, inverse_degrees{.005555555555555555};
double constexpr gravity{9.81}, nozzle_degrees{20};
double constexpr trim_minimum_speed{20}, flap_minimum_speed{10}, limit_minimum_speed{1};
double constexpr bank_cosine_minimum{.01}, bank_limit_degrees{45};
double constexpr roll_limit_scale{-1.1}, aileron_damage_scale{2.8}, elevator_damage_scale{.03};
double constexpr damage_roll_frequency{10}, damage_pitch_frequency{5};
double constexpr vector_zero_speed{60}, vector_full_speed{90}, vector_speed_scale{.03333333333333333};
double constexpr eurofighter_speed_target{1.7}, eurofighter_limit_mach{2};
double constexpr nighthawk_speed_target{.8}, nighthawk_limit_mach{.9};
double constexpr stall_trim_scale{.02}, degrees_per_radian{180.0 / native_pi};
double constexpr nozzle_angle{native_pi * nozzle_degrees * inverse_degrees};
uint16_t constexpr external_control_mode{3};

calculation asin(calculation const value) { return std::atan2(value,std::sqrt(1.0 - value * value)); }
calculation velocity_area(aerodynamic_airdata const &data, double const area) {
  calculation const speed{data.metres_per_second};
  return static_cast<calculation>(data.atmosphere.density) * speed * speed * area;
}
calculation thrust_moment(aerodynamic_manual_state const &state, aerodynamic_powerplant const &powerplant, aerodynamic_manual_context const &context) {
  return std::sin(nozzle_angle) * (static_cast<calculation>(powerplant.thrust) * state.thrust_vector_fraction) * context.engine_pitch_arm * 2;
}
afterburner_change overspeed(aerodynamic_manual_state &state, aerodynamic_powerplant &powerplant, aerodynamic_manual_context const &context, double const target) {
  aerodynamic_speed_state speed{
    .target_metres_per_second{target},
    .air_brake_fraction{state.air_brake_fraction},
    .air_brake_condition{state.air_brake_condition},
  };
  aerodynamic_speed_context const inputs{
    .metres_per_second{context.data.metres_per_second},
    .mass_kg{context.data.mass_kg},
    .forward_force{context.body_forward_force},
    .air_brake_drag{context.geometry.air_brake_drag},
    .afterburner_stages{context.afterburner_stages},
  };
  auto const change{update_aerodynamic_speed(speed,powerplant,inputs)};
  state.air_brake_fraction = speed.air_brake_fraction;
  state.air_brake_condition = speed.air_brake_condition;
  return change;
}

} // anonymous namespace

afterburner_change update_aerodynamic_manual(aerodynamic_manual_state &state, aerodynamic_powerplant &powerplant, aerodynamic_manual_context const &context) {
  auto &controls{state.controls};
  auto const &data{context.data};
  auto const &c{context.coefficients};
  auto const &p{context.geometry};
  calculation const speed{data.metres_per_second}, alpha{data.angle_of_attack};
  bool const trim{speed > trim_minimum_speed && (context.source == control_source::joystick || (context.source == control_source::keyboard && controls.keyboard_elevator == 0))};
  if(trim) {
    if(context.stick_pressure && (!controls.autopilot.enabled || controls.autopilot.mode == external_control_mode)) {
      double const pitch{static_cast<double>(-asin(data.basis.forward.z))};
      auto const cosine{std::cos(pitch)};
      double const stored_cosine{static_cast<double>(cosine)};
      double bank{0};
      if(cosine > bank_cosine_minimum || stored_cosine < -bank_cosine_minimum) {
        double const divisor{static_cast<double>(std::cos(pitch))};
        double const ratio{static_cast<double>(static_cast<calculation>(data.basis.right.z) / divisor)};
        if(ratio >= -1 && ratio <= 1) bank = static_cast<double>(asin(ratio));
      }
      double const positive{native_pi * bank_limit_degrees * inverse_degrees};
      double const negative{native_pi * -bank_limit_degrees * inverse_degrees};
      if(bank > positive) bank = positive;
      else if(bank < negative) bank = negative;
      state.pitch_rate_bias = static_cast<double>(std::sin(bank) * gravity * std::tan(bank) / speed);
    } else state.pitch_rate_bias = 0;
    calculation const dynamic{velocity_area(data,p.wing_area) * p.mean_chord};
    if(context.aircraft == player_aircraft::raptor) state.elevator_trim = static_cast<double>(-(alpha * c.pitch_stability + p.zero_pitch_moment) / (thrust_moment(state,powerplant,context) / dynamic + static_cast<calculation>(c.elevator_moment) * p.maximum_elevator_angle));
    else state.elevator_trim = static_cast<double>((alpha * c.pitch_stability + p.zero_pitch_moment) * (-1.0 / (static_cast<calculation>(c.elevator_moment) * p.maximum_elevator_angle)));
    if(context.military) {
      calculation const denominator{thrust_moment(state,powerplant,context) / dynamic + static_cast<calculation>(c.elevator_moment) * p.maximum_elevator_angle};
      state.elevator_trim = static_cast<double>(state.elevator_trim - (static_cast<calculation>(state.pitch_rate_bias) * c.pitch_damping * p.mean_chord / (speed * 2)) / denominator);
    }
  }
  return finish_aerodynamic_controls(state,powerplant,context);
}

afterburner_change finish_aerodynamic_controls(aerodynamic_manual_state &state, aerodynamic_powerplant &powerplant, aerodynamic_manual_context const &context) {
  auto &controls{state.controls};
  auto const &data{context.data};
  auto const &c{context.coefficients};
  auto const &p{context.geometry};
  calculation const speed{data.metres_per_second}, alpha{data.angle_of_attack};
  if(speed > flap_minimum_speed && context.aircraft != player_aircraft::nighthawk && context.flaps_condition != system_condition::failed) {
    state.flap_fraction = static_cast<double>((static_cast<calculation>(data.mass_kg) * 2 * gravity / velocity_area(data,p.wing_area) - p.zero_lift) / (static_cast<calculation>(p.flap_lift_gradient) * p.maximum_flap_angle));
    if(state.flap_fraction > 1) state.flap_fraction = 1;
    if(state.flap_fraction < 0) state.flap_fraction = 0;
  } else state.flap_fraction = 0;
  afterburner_change change{afterburner_change::none};
  if(context.aircraft == player_aircraft::eurofighter) {
    double const target{static_cast<double>(static_cast<calculation>(data.atmosphere.sound_metres_per_second) * eurofighter_speed_target)};
    if(alpha > p.critical_angle) state.elevator_trim = static_cast<double>(state.elevator_trim - (alpha - p.critical_angle) * degrees_per_radian * stall_trim_scale * controls.minimum_elevator);
    if(data.mach > eurofighter_limit_mach) change = overspeed(state,powerplant,context,target);
  }
  if(context.aircraft == player_aircraft::nighthawk && data.mach > nighthawk_limit_mach) change = overspeed(state,powerplant,context,static_cast<double>(static_cast<calculation>(data.atmosphere.sound_metres_per_second) * nighthawk_speed_target));
  controls.elevator = static_cast<double>(static_cast<calculation>(state.elevator_trim) + controls.elevator);
  if(speed > limit_minimum_speed) {
    calculation const dynamic{velocity_area(data,p.wing_area)};
    state.positive_g_lift_limit = static_cast<double>((static_cast<calculation>(context.maximum_g) - 1) * 2 * data.mass_kg * gravity / dynamic);
    if(state.positive_g_lift_limit > context.surfaces.positive_lift_limit) state.positive_g_lift_limit = context.surfaces.positive_lift_limit;
    state.negative_g_lift_limit = static_cast<double>(static_cast<calculation>(context.minimum_g) * 2 * data.mass_kg * gravity / dynamic);
    if(state.negative_g_lift_limit < context.surfaces.negative_lift_limit) state.negative_g_lift_limit = context.surfaces.negative_lift_limit;
    calculation const damping{static_cast<calculation>(c.pitch_damping) * p.mean_chord / (speed * 2) * data.mass_kg * gravity};
    calculation const inverse_mass_speed{1.0 / (static_cast<calculation>(data.mass_kg) * speed)};
    calculation const inverse_lift{1.0 / c.lift_slope};
    calculation const positive_static{(static_cast<calculation>(state.positive_g_lift_limit) - context.surfaces.flap_lift_offset) * c.pitch_stability * inverse_lift + p.zero_pitch_moment};
    calculation const negative_static{(static_cast<calculation>(state.negative_g_lift_limit) - context.surfaces.flap_lift_offset) * c.pitch_stability * inverse_lift + p.zero_pitch_moment};
    double const thrust_scale{static_cast<double>(static_cast<calculation>(powerplant.thrust) * 2 * state.thrust_vector_fraction)};
    calculation const thrust{std::sin(nozzle_angle) * thrust_scale * context.engine_pitch_arm * (1.0 / (dynamic * p.mean_chord))};
    calculation const inverse_control{1.0 / (thrust + static_cast<calculation>(c.elevator_moment) * p.maximum_elevator_angle)};
    controls.minimum_elevator = static_cast<double>(-(damping * context.maximum_g * inverse_mass_speed + positive_static) * inverse_control);
    controls.maximum_elevator = static_cast<double>(-(damping * context.minimum_g * inverse_mass_speed + negative_static) * inverse_control);
    if(controls.minimum_elevator < -1) controls.minimum_elevator = -1;
    if(controls.maximum_elevator > 1) controls.maximum_elevator = 1;
    if(controls.maximum_elevator > -controls.minimum_elevator) controls.maximum_elevator = -controls.minimum_elevator;
    double const force{static_cast<double>(-static_cast<calculation>(context.body_down_force))};
    double const weight{static_cast<double>(static_cast<calculation>(data.mass_kg) * gravity)};
    double const current_g{static_cast<double>(static_cast<calculation>(force) / weight)};
    double const margin{static_cast<double>(static_cast<calculation>(context.minimum_g) - current_g)};
    if(margin < 0) {
      double const roll_damping{static_cast<double>(static_cast<calculation>(c.roll_damping) * p.wing_span / (speed * 2))};
      double const squared{static_cast<double>(static_cast<calculation>(margin) / context.cockpit_upward_metres * gravity)};
      controls.aileron_limit = static_cast<double>(std::sqrt(static_cast<calculation>(squared)) * roll_limit_scale * roll_damping / (static_cast<calculation>(c.aileron_effectiveness) * p.maximum_aileron_angle));
    }
    if(controls.aileron_limit > 1) controls.aileron_limit = 1;
    if(context.accumulated_damage != 0) {
      calculation const damage{context.accumulated_damage}, time{data.simulation_seconds};
      controls.aileron = static_cast<double>(static_cast<calculation>(controls.aileron_limit) * aileron_damage_scale * damage * std::sin(time * damage_roll_frequency) + controls.aileron);
      controls.elevator = static_cast<double>(static_cast<calculation>(controls.minimum_elevator) * elevator_damage_scale * damage * std::sin(time * (damage * damage_pitch_frequency) + damage) + controls.elevator);
    }
    if(controls.elevator < controls.minimum_elevator) controls.elevator = controls.minimum_elevator;
    if(controls.elevator > controls.maximum_elevator) controls.elevator = controls.maximum_elevator;
  }
  if(controls.aileron > controls.aileron_limit) controls.aileron = controls.aileron_limit;
  if(controls.aileron < -controls.aileron_limit) controls.aileron = -controls.aileron_limit;
  state.thrust_vector_fraction = 0;
  if(context.aircraft == player_aircraft::raptor) {
    powerplant.nozzle_angles = vec2d{0,0};
    if(powerplant.engine_factor.x != 0 && powerplant.engine_factor.y != 0) {
      if(speed < vector_full_speed) {
        state.thrust_vector_fraction = static_cast<double>((speed - vector_zero_speed) * vector_speed_scale);
        if(speed < vector_zero_speed) state.thrust_vector_fraction = 0;
      } else state.thrust_vector_fraction = 1;
      auto const sine{std::sin(nozzle_angle)};
      double const pitch{static_cast<double>(-asin(static_cast<calculation>(controls.elevator) * state.thrust_vector_fraction * sine))};
      auto const roll{asin(static_cast<calculation>(controls.aileron) * state.thrust_vector_fraction * sine)};
      powerplant.nozzle_angles.x = static_cast<double>(static_cast<calculation>(pitch) + roll);
      powerplant.nozzle_angles.y = static_cast<double>(static_cast<calculation>(pitch) - roll);
      calculation const limit{static_cast<calculation>(state.thrust_vector_fraction) * nozzle_degrees * native_pi * inverse_degrees};
      for(auto &angle : powerplant.nozzle_angles) {
        // Native compares the scaled limit but assigns the full +/-20 degrees.
        if(angle > limit) angle = nozzle_angle;
        if(angle < -limit) angle = -nozzle_angle;
      }
    }
  }
  return change;
}

} // namespace tfx::flight
