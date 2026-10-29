#include "math/floating_environment.h"
#include "simple_ground.h"
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = long double;
int constexpr stall_pitch_rate{24};
double constexpr stall_speed_interval{50}, stall_pitch_fraction{.02};
float constexpr stall_pitch_scale{1024}, low_speed_sink_per_tick{.1f};
float constexpr indicated_speed_scale{.515f};
int16_t constexpr carrier_surface{3};

int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0L) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void rotate_body(simple_ground_state &state, math::rotation_axis const axis, float const amount) {
  auto &remainder{state.rotation_remainder[static_cast<unsigned int>(axis)]};
  calculation const accumulated{static_cast<calculation>(remainder) + amount};
  remainder = static_cast<float>(accumulated);
  auto const rotation{static_cast<math::angle>(truncate(accumulated))};
  remainder = static_cast<float>(static_cast<calculation>(remainder) - math::signed_word(rotation));
  state.body.rotate(axis,rotation);
}

} // anonymous namespace

void update_simple_stall_attitude(simple_ground_state &state, simple_ground_context const &context) {
  math::floating_environment const environment;
  int16_t pitch{state.stall_pitch};
  if(!state.landed) {
    if(static_cast<calculation>(context.low_speed_reference) + stall_speed_interval > context.turn_metres_per_second && !context.pitch_inhibited) {
      calculation const response{1.0L - (static_cast<calculation>(context.turn_metres_per_second) - context.low_speed_reference) * stall_pitch_fraction};
      int16_t const desired{word(truncate(response * -stall_pitch_scale))};
      if(pitch > desired) pitch = word(pitch - static_cast<int32_t>(context.elapsed) * stall_pitch_rate);
      // The second calculation stores its product as double. Its comparison
      // retains the extended response; only the final assignment reloads a float.
      double const stored_difference{static_cast<double>((static_cast<calculation>(context.turn_metres_per_second) - context.low_speed_reference) * stall_pitch_fraction)};
      calculation const second_response{1.0L - stored_difference};
      float const rounded_response{static_cast<float>(second_response)};
      int16_t const second_desired{word(truncate(second_response * -stall_pitch_scale))};
      if(pitch < second_desired) pitch = word(-word(truncate(static_cast<calculation>(rounded_response) * stall_pitch_scale)));
    } else {
      if(pitch < 0) pitch = word(pitch + static_cast<int32_t>(context.elapsed) * stall_pitch_rate);
      if(pitch > 0) pitch = 0;
    }
  }
  state.stall_pitch = pitch;
  state.visible.rotate(math::rotation_axis::pitch,static_cast<math::angle>(pitch));
}
void update_simple_ground(simple_ground_state &state, simple_ground_context const &context) {
  math::floating_environment const environment;
  state.indicated_speed = static_cast<double>(static_cast<calculation>(context.reported_speed) * indicated_speed_scale);
  state.low_speed_reference = context.low_speed_reference;
  state.previous_landed = state.landed;
  if(context.metres_per_second < context.stall_metres_per_second) state.position_y = static_cast<float>(static_cast<calculation>(context.elapsed) * low_speed_sink_per_tick + state.position_y);
  calculation const ground_y{static_cast<calculation>(context.ground.height) - context.gear_height};
  if(context.ground.present && ground_y <= state.position_y) {
    if(context.gear_down && state.body.down.y >= 0) state.low_speed_ground_contact = context.fixed_speed < static_cast<calculation>(context.rollout_speed_threshold) * 2.0;
    else {
      state.position_y = context.ground.height;
      state.collision = 2;
    }
    if(!context.pitch_down || context.metres_per_second < context.stall_metres_per_second) {
      state.position_y = static_cast<float>(ground_y);
      auto const angles{state.visible.angles()};
      state.landing_angles.x = static_cast<int16_t>((state.landing_angles.x + math::signed_word(angles.x)) / 2);
      state.landing_angles.y = static_cast<int16_t>((state.landing_angles.y + math::signed_word(angles.z)) / 2);
    }
    rotate_body(state,math::rotation_axis::pitch,static_cast<float>(-state.landing_angles.x));
    rotate_body(state,math::rotation_axis::roll,static_cast<float>(-state.landing_angles.y));
    state.landed = true;
    if(context.surface_type == carrier_surface && context.arrestor_hook) state.arrested = true;
  } else if(state.position_y > 0) {
    state.position_y = context.ground.height;
    state.collision = 1;
  } else {
    state.landing_angles.x = static_cast<int16_t>(state.landing_angles.x / 2);
    state.landing_angles.y = static_cast<int16_t>(state.landing_angles.y / 2);
    state.landed = false;
    state.low_speed_ground_contact = false;
  }
}

} // namespace tfx::flight
