#include "aerodynamic_controls.h"

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr maximum_rudder_speed{205}, rudder_retention{.8};
double constexpr mouse_pitch_scale{.0022222222222222222}, mouse_roll_scale{.0025};
double constexpr joystick_scale{.001953125};
double constexpr idle_throttle{.6}, throttle_increase{.01}, throttle_decrease{-.01};
double constexpr upper_target_altitude{-19812}, lower_target_altitude{-304.8};
double constexpr target_altitude_step{3.048}, target_speed_step{.515}, minimum_target_speed{52};
int constexpr mouse_pitch_dead_zone{52}, mouse_roll_dead_zone{112};
unsigned int constexpr target_heading_step{182};

void rudder_keys(aerodynamic_control_state &state, aerodynamic_control_context const &context, bool const decay) {
  if(context.keys.left) {
    state.keyboard_rudder = static_cast<double>(static_cast<calculation>(state.keyboard_rudder) - context.rudder_increment);
    if(state.keyboard_rudder < -1) state.keyboard_rudder = -1;
  } else if(context.keys.right) {
    state.keyboard_rudder = static_cast<double>(static_cast<calculation>(state.keyboard_rudder) + context.rudder_increment);
    if(state.keyboard_rudder > 1) state.keyboard_rudder = 1;
  } else state.keyboard_rudder = decay ? static_cast<double>(static_cast<calculation>(state.keyboard_rudder) * rudder_retention) : 0;
}
int mouse_axis(int value, int const dead_zone) {
  if(value < dead_zone && value > -dead_zone) value = 0;
  if(value > 0) value -= dead_zone;
  if(value < 0) value += dead_zone;
  return value;
}

} // anonymous namespace

bool update_aerodynamic_controls(aerodynamic_control_state &state, aerodynamic_powerplant &powerplant, aerodynamic_control_context const &context) {
  if(context.source == control_source::keyboard) {
    if(context.keys.down) {
      state.keyboard_elevator = static_cast<double>(state.keyboard_elevator - (static_cast<calculation>(state.elevator) - state.minimum_elevator) * context.pitch_gain * context.seconds);
      if(state.keyboard_elevator < -1) state.keyboard_elevator = -1;
    } else if(context.keys.up) {
      state.keyboard_elevator = static_cast<double>((static_cast<calculation>(state.maximum_elevator) - state.elevator) * (static_cast<calculation>(context.pitch_gain) * 2) * context.seconds + state.keyboard_elevator);
      if(state.keyboard_elevator > 1) state.keyboard_elevator = 1;
    } else state.keyboard_elevator = 0;
    state.elevator = state.keyboard_elevator;
    if(!context.alt && !context.low_speed_ground_contact) {
      state.keyboard_rudder = 0;
      if(context.keys.left) {
        state.keyboard_aileron = static_cast<double>(state.keyboard_aileron - (static_cast<calculation>(state.aileron_limit) + state.aileron) * context.roll_gain);
        if(state.keyboard_aileron < -state.aileron_limit) state.keyboard_aileron = -state.aileron_limit;
      } else if(context.keys.right) {
        state.keyboard_aileron = static_cast<double>((static_cast<calculation>(state.aileron_limit) - state.aileron) * context.roll_gain + state.keyboard_aileron);
        if(state.keyboard_aileron > state.aileron_limit) state.keyboard_aileron = state.aileron_limit;
      } else state.keyboard_aileron = 0;
    } else {
      state.keyboard_aileron = 0;
      if(context.metres_per_second < maximum_rudder_speed) rudder_keys(state,context,true);
    }
    state.aileron = state.keyboard_aileron;
    state.rudder = state.keyboard_rudder;
  } else if(context.source == control_source::mouse) {
    int const y{mouse_axis(context.mouse.y,mouse_pitch_dead_zone)};
    int const x{mouse_axis(context.mouse.x,mouse_roll_dead_zone)};
    state.elevator = static_cast<double>(-static_cast<calculation>(y) * mouse_pitch_scale);
    if(state.minimum_elevator < -1) state.minimum_elevator = -1;
    if(state.maximum_elevator > 1) state.maximum_elevator = 1;
    if(state.aileron_limit > 1) state.aileron_limit = 1;
    if(state.elevator < 0) state.elevator = static_cast<double>(static_cast<calculation>(state.elevator) * state.elevator * state.elevator * -state.minimum_elevator);
    if(state.elevator > 0) state.elevator = static_cast<double>(static_cast<calculation>(state.elevator) * state.elevator * state.elevator * state.maximum_elevator);
    state.aileron = static_cast<double>(static_cast<calculation>(x) * mouse_roll_scale);
    state.aileron = static_cast<double>(static_cast<calculation>(state.aileron) * state.aileron * state.aileron * state.aileron_limit);
    state.keyboard_aileron = 0;
    if(context.metres_per_second < maximum_rudder_speed) rudder_keys(state,context,false);
    // This native branch retains the actual rudder; it does not copy its key target.
  } else if(context.source == control_source::joystick) {
    calculation const y{static_cast<calculation>(context.joystick.y)};
    calculation const pitch{context.joystick.y > 0 ? (static_cast<calculation>(state.minimum_elevator) - context.elevator_trim) * y * joystick_scale * y * joystick_scale * context.joystick_gain : -(static_cast<calculation>(state.maximum_elevator) - context.elevator_trim) * y * joystick_scale * y * joystick_scale * -static_cast<calculation>(context.joystick_gain)};
    state.elevator = static_cast<double>(pitch);
    state.aileron = static_cast<double>(static_cast<calculation>(context.joystick.x) * state.aileron_limit * joystick_scale);
    state.aileron = static_cast<double>(static_cast<calculation>(context.joystick_gain) * state.aileron);
    if(state.aileron > state.aileron_limit) state.aileron = state.aileron_limit;
    if(state.aileron < -state.aileron_limit) state.aileron = -state.aileron_limit;
    state.keyboard_elevator = state.elevator;
    state.keyboard_aileron = state.aileron;
    if(context.metres_per_second < maximum_rudder_speed && (context.keys.left || context.keys.right)) rudder_keys(state,context,true);
    else state.keyboard_rudder = static_cast<double>(static_cast<calculation>(state.keyboard_rudder) * rudder_retention);
    state.rudder = state.keyboard_rudder;
  }
  bool stopped{false};
  bool const running{powerplant.engine_factor.x != 0 || powerplant.engine_factor.y != 0};
  if(!state.autopilot.enabled && (context.decelerate || !running)) {
    calculation const throttle{static_cast<calculation>(powerplant.throttle) + throttle_decrease};
    powerplant.throttle = static_cast<float>(throttle);
    if(throttle < idle_throttle && running) powerplant.throttle = static_cast<float>(idle_throttle);
    if(powerplant.throttle < 0) powerplant.throttle = 0;
    if(!running) stopped = powerplant.stop_afterburner();
  }
  if(!state.autopilot.enabled && running && (context.accelerate || powerplant.throttle < idle_throttle)) {
    powerplant.throttle = static_cast<float>(static_cast<calculation>(powerplant.throttle) + throttle_increase);
    if(powerplant.throttle > 1) powerplant.throttle = 1;
  }
  if(powerplant.afterburner) powerplant.throttle = 1;
  if(state.autopilot.enabled) {
    if(state.autopilot.mode == 0 || state.autopilot.mode == 1) {
      if(context.keys.down && state.autopilot.down_metres < lower_target_altitude) state.autopilot.down_metres = static_cast<double>(static_cast<calculation>(state.autopilot.down_metres) + target_altitude_step);
      else if(context.keys.up && state.autopilot.down_metres > upper_target_altitude) state.autopilot.down_metres = static_cast<double>(static_cast<calculation>(state.autopilot.down_metres) - target_altitude_step);
      if(state.autopilot.mode == 1) {
        if(context.keys.left) state.autopilot.heading = static_cast<math::angle>(state.autopilot.heading - target_heading_step);
        else if(context.keys.right) state.autopilot.heading = static_cast<math::angle>(state.autopilot.heading + target_heading_step);
      }
    }
    if(context.accelerate && state.autopilot.metres_per_second < context.maximum_autopilot_speed) state.autopilot.metres_per_second = static_cast<double>(static_cast<calculation>(state.autopilot.metres_per_second) + target_speed_step);
    else if(context.decelerate && state.autopilot.metres_per_second > minimum_target_speed) state.autopilot.metres_per_second = static_cast<double>(static_cast<calculation>(state.autopilot.metres_per_second) - target_speed_step);
  }
  return stopped;
}

} // namespace tfx::flight
