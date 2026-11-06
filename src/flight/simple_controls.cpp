#include "simple_controls.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
float constexpr native_pi{3.141590118408203f};
float constexpr radians_per_angle{50.0f / 32768.0f};
float constexpr seconds_per_tick{.02f}, angles_per_radian{32768};
float constexpr normalised_word{3.0518509447574615e-05f};
double constexpr gravity{9.81}, response_scale{5.0}, response_fraction{1.0 / 3.0};
float constexpr low_speed_limit{150}, roll_limit_multiplier{3};
float constexpr keyboard_response{.25f};
double constexpr keyboard_retention{.85};
float constexpr joystick_scale{1.0f / 512};
int constexpr joystick_direction_threshold{40}, alt_rate{40};
int32_t constexpr rolling_rudder_speed{400};

calculation normalised(int32_t const component) {
  return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(component))) * normalised_word;
}
float approach(float const current, float const target, simulation::timer_ticks const ticks) {
  return static_cast<float>((static_cast<calculation>(target) - current) * ticks * keyboard_response * keyboard_response + current);
}

} // anonymous namespace

direction_keys update_simple_controls(simple_control_state &state, simple_control_context const &context) {
  auto keys{context.keys};
  if(context.fixed_speed <= 1) return keys;
  float pitch{static_cast<float>(static_cast<calculation>(state.angular_rate.x) * radians_per_angle * native_pi)};
  float roll{static_cast<float>(static_cast<calculation>(state.angular_rate.z) * radians_per_angle * native_pi)};
  state.negative_pitch_limit = static_cast<float>(-static_cast<calculation>(context.maximum_g) * response_scale * response_fraction * gravity / context.metres_per_second);
  if(context.metres_per_second < low_speed_limit) {
    // Native 0x8d01e uses the literal -9 even for the F-117's six-G preset.
    float constexpr low_speed_fraction{1.0f / 150}, low_speed_g{-9}, low_speed_third{1.0f / 3};
    double constexpr low_speed_tail{1.0 / 150};
    state.negative_pitch_limit = static_cast<float>(static_cast<calculation>(context.metres_per_second) * low_speed_fraction * low_speed_g * 5.0f * low_speed_third * gravity * low_speed_tail);
  }
  double const vertical_gravity{static_cast<double>(normalised(context.orientation.down.y) * gravity * response_scale * response_fraction / context.metres_per_second)};
  auto const bank_coupling{normalised(context.orientation.right.y) * -context.coordinated_turn_rate};
  state.negative_pitch_limit = static_cast<float>(bank_coupling + vertical_gravity + state.negative_pitch_limit);
  double const negative_g{static_cast<double>(-static_cast<calculation>(context.minimum_g) * response_scale * response_fraction * gravity / context.metres_per_second)};
  double const negative_bank{static_cast<double>(negative_g - normalised(context.orientation.right.y) * context.coordinated_turn_rate)};
  state.positive_pitch_limit = static_cast<float>(normalised(context.orientation.down.y) * gravity * response_scale * response_fraction / context.metres_per_second + negative_bank);
  if(state.positive_pitch_limit > -state.negative_pitch_limit) state.positive_pitch_limit = -state.negative_pitch_limit;
  state.roll_limit = static_cast<float>(-static_cast<calculation>(state.negative_pitch_limit) * roll_limit_multiplier);
  if(context.source == control_source::keyboard) {
    if(!context.landed || context.metres_per_second > context.stall_metres_per_second) {
      if(!context.pitch_inhibited && keys.up) pitch = approach(pitch,state.positive_pitch_limit,context.elapsed);
      else if(!context.pitch_inhibited && keys.down) pitch = approach(pitch,state.negative_pitch_limit,context.elapsed);
      else pitch = static_cast<float>(std::pow(static_cast<calculation>(keyboard_retention),static_cast<calculation>(context.elapsed)) * pitch);
    }
    if(!context.alt && (!context.low_speed_ground_contact || context.fixed_speed > rolling_rudder_speed)) {
      state.angular_rate.y = 0;
      if(keys.left) roll = approach(roll,state.roll_limit,context.elapsed);
      else if(keys.right) roll = approach(roll,-state.roll_limit,context.elapsed);
      else roll = static_cast<float>(std::pow(static_cast<calculation>(keyboard_retention),static_cast<calculation>(context.elapsed)) * roll);
    }
  }
  if(context.source == control_source::joystick) {
    if(context.joystick.y < -joystick_direction_threshold) keys.up = true;
    else if(context.joystick.y > joystick_direction_threshold) keys.down = true;
    if(context.joystick.x < -joystick_direction_threshold) keys.left = true;
    else if(context.joystick.x > joystick_direction_threshold) keys.right = true;
    pitch = static_cast<float>(static_cast<calculation>(-static_cast<int>(context.joystick.y)) * state.positive_pitch_limit * joystick_scale);
    if(context.landed && context.metres_per_second < context.stall_metres_per_second) pitch = 0;
    roll = static_cast<float>(static_cast<calculation>(-static_cast<int>(context.joystick.x)) * state.roll_limit * joystick_scale);
  }
  if(context.alt) {
    int16_t const rate{context.low_speed_ground_contact ? math::signed_word(static_cast<uint16_t>(context.fixed_speed)) : int16_t{alt_rate}};
    state.angular_rate.y = keys.left ? static_cast<float>(-rate) : keys.right ? rate : 0;
    roll = static_cast<float>(static_cast<calculation>(roll) * .5f);
  }
  auto const inverse_pi{1.0 / native_pi};
  state.angular_rate.x = static_cast<float>(static_cast<calculation>(pitch) * seconds_per_tick * angles_per_radian * inverse_pi);
  state.angular_rate.z = static_cast<float>(static_cast<calculation>(roll) * seconds_per_tick * angles_per_radian * inverse_pi);
  return keys;
}

} // namespace tfx::flight
