#include "aerodynamic_powerplant.h"
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr idle_throttle{.6}, throttle_span{2.5}, half{.5};
double constexpr gravity{9.81}, minimum_flow{.5}, fuel_use_scale{.7};
double constexpr fuel_display_scale{9999}, engine_decay{-.0001}, engine_drop{-.1};
unsigned int constexpr warp_multiplier{5}, engine_drop_threshold{64000};
uint8_t constexpr afterburner_off{3};

int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}

} // anonymous namespace

bool aerodynamic_powerplant::stop_afterburner() {
  if(!afterburner) return false;
  afterburner = 0;
  afterburner_control = afterburner_display = afterburner_off;
  return true;
}

afterburner_change aerodynamic_powerplant::increase_afterburner(unsigned int const stages) {
  if(afterburner >= stages || (engine_factor.x == 0 && engine_factor.y == 0) || fuel <= 0) return afterburner_change::none;
  auto const change{afterburner ? afterburner_change::increased : afterburner_change::started};
  throttle_target = 1;
  throttle = 1;
  afterburner_control = afterburner_display = afterburner_off;
  ++afterburner;
  return change;
}
afterburner_change aerodynamic_powerplant::decrease_afterburner() {
  if(!afterburner) return afterburner_change::none;
  --afterburner;
  afterburner_control = afterburner_display = afterburner_off;
  return afterburner ? afterburner_change::decreased : afterburner_change::stopped;
}

void update_aerodynamic_thrust(aerodynamic_powerplant &state, aerodynamic_powerplant_parameters const &p, aerodynamic_powerplant_context const &context) {
  state.thrust = static_cast<double>((static_cast<calculation>(state.throttle) - idle_throttle) * p.dry_thrust * throttle_span);
  if(p.aircraft != player_aircraft::nighthawk) state.thrust = static_cast<double>(static_cast<calculation>(state.afterburner) * p.afterburner_thrust / p.afterburner_stages + state.thrust);
  if(state.thrust < 0) state.thrust = 0;
  calculation const altitude{static_cast<calculation>(context.altitude_km) / p.ceiling_km};
  state.thrust = static_cast<double>((1.0 - altitude * altitude) * state.thrust);
  if(state.thrust < 0) state.thrust = 0;
  state.thrust = static_cast<double>((static_cast<calculation>(state.engine_factor.x) + state.engine_factor.y) * state.thrust * half);
  if(p.aircraft == player_aircraft::raptor) {
    auto const left_cosine{std::cos(state.nozzle_angles.x)};
    double const left_sine{static_cast<double>(std::sin(state.nozzle_angles.x))};
    auto const right_cosine{std::cos(state.nozzle_angles.y)};
    double const right_sine{static_cast<double>(std::sin(state.nozzle_angles.y))};
    state.engine_force.x = static_cast<double>((right_cosine + left_cosine) * state.thrust * half);
    calculation const vertical{-static_cast<calculation>(state.thrust) * (static_cast<calculation>(left_sine) + right_sine)};
    state.engine_force.z = static_cast<double>(vertical * half);
    state.engine_moment.x = static_cast<double>((static_cast<calculation>(left_sine) - right_sine) * state.thrust * p.roll_thrust_arm * half);
    state.engine_moment.y = static_cast<double>(vertical * p.pitch_thrust_arm * half);
  } else state.engine_force.x = state.thrust;
}

bool consume_aerodynamic_fuel(aerodynamic_powerplant &state, aerodynamic_powerplant_parameters const &p, aerodynamic_powerplant_context const &context) {
  double const inverse_gravity{1.0 / gravity};
  state.fuel_flow = static_cast<double>(static_cast<calculation>(state.thrust) * p.dry_fuel_coefficient * inverse_gravity);
  if(state.afterburner) {
    calculation const reheat{static_cast<calculation>(state.afterburner) * p.afterburner_thrust / p.afterburner_stages};
    state.fuel_flow = static_cast<double>(reheat / (static_cast<calculation>(p.dry_thrust) + reheat) * state.thrust * p.afterburner_fuel_coefficient * inverse_gravity);
  }
  calculation const altitude{static_cast<calculation>(context.altitude_km) / p.ceiling_km};
  calculation const floor{(1.0 - altitude * altitude) * minimum_flow};
  if(floor > state.fuel_flow && (state.engine_factor.x != 0 || state.engine_factor.y != 0)) state.fuel_flow = static_cast<double>(floor);
  state.fuel = static_cast<double>(state.fuel - static_cast<calculation>(state.fuel_flow) * context.seconds * (context.time_warp ? warp_multiplier : 1) * fuel_use_scale);
  if(state.fuel < 0) state.fuel = 0;
  auto const whole_fuel{math::signed_word(static_cast<uint16_t>(truncate(state.fuel)))};
  state.fuel_readout = math::signed_word(static_cast<uint16_t>(truncate(static_cast<calculation>(whole_fuel) / (static_cast<calculation>(state.fuel_capacity) + state.extra_fuel_capacity) * fuel_display_scale)));
  bool stopped{false};
  if(state.fuel == 0) {
    stopped = state.stop_afterburner();
    state.engine_factor = vec2d{0,0};
  }
  return stopped;
}

void update_aerodynamic_engine_faults(aerodynamic_powerplant &state, math::random_state &random) {
  for(unsigned int engine{0}; engine != state.engine_condition.size(); ++engine) {
    auto &condition{state.engine_condition[engine]};
    auto &factor{state.engine_factor[engine]};
    if(condition == system_condition::intermittent) {
      factor = static_cast<double>(static_cast<calculation>(factor) + engine_decay);
      if(random.next() > engine_drop_threshold) factor = static_cast<double>(static_cast<calculation>(factor) + engine_drop);
      if(factor <= 0) condition = system_condition::failed;
    }
    if(condition == system_condition::failed) factor = 0;
  }
}

} // namespace tfx::flight
