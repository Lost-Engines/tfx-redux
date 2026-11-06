#include "joystick.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {
unsigned int constexpr combination_count{16};
int16_t constexpr native_axis_limit{512}, firing_repeat_ticks{10}, command_repeat_ticks{25};
struct binding {
  joystick_action action{joystick_action::none};
  int16_t repeat{0};
};
constexpr auto bindings(bool const flightstick) {
  std::array<binding,combination_count> result{};
  result[7] = {
    .action{joystick_action::fire},
    .repeat{firing_repeat_ticks},
  };
  result[11] = {
    .action{joystick_action::ground_weapon},
    .repeat{command_repeat_ticks},
  };
  result[13] = {
    .action{joystick_action::air_weapon},
    .repeat{command_repeat_ticks},
  };
  result[14] = {
    .action{joystick_action::cycle_target},
    .repeat{command_repeat_ticks},
  };
  if(flightstick) {
    std::array constexpr views{joystick_action::front_view,joystick_action::rear_view,joystick_action::right_view,joystick_action::left_view};
    for(unsigned int index{0}; index != views.size(); ++index) result[index] = {
      .action{views[index]},
      .repeat{command_repeat_ticks},
    };
  }
  return result;
}
std::array constexpr profiles{bindings(false),bindings(true)};
} // anonymous namespace

vec2<int16_t> joystick_axes(vec2f const &normalised, float const scale) {
  vec2<int16_t> result;
  for(unsigned int axis{0}; axis != 2; ++axis) {
    float const ratio{std::clamp(normalised[axis],-1.0f,1.0f)};
    double const scaled{std::abs(static_cast<double>(ratio)) * ratio * scale};
    int32_t const integer{std::isfinite(scaled) && scaled >= std::numeric_limits<int32_t>::min() && scaled < 2147483648.0 ? static_cast<int32_t>(scaled) : std::numeric_limits<int32_t>::min()};
    result[axis] = std::clamp(math::signed_word(static_cast<uint16_t>(integer)),static_cast<int16_t>(-native_axis_limit),native_axis_limit);
  }
  return result;
}

joystick_action joystick_buttons::advance(uint8_t const pressed, uint8_t const stick_type, simulation::timer_ticks const elapsed) {
  // Native 0x86530: timers are signed words; subtraction can wrap.
  for(auto &profile : remaining) for(auto &timer : profile) timer = timer > 0 ? math::signed_word(static_cast<uint16_t>(timer - elapsed)) : 0;
  unsigned int index{combination_count - 1};
  for(unsigned int button{0}; button != 4; ++button) if(pressed & (1u << button)) index ^= 8u >> button;
  unsigned int const profile{stick_type == 1 ? 1u : 0u};
  auto &timer{remaining[profile][index]};
  if(timer > 0) return joystick_action::none;
  auto const &command{profiles[profile][index]};
  timer = command.repeat;
  return command.action;
}

} // namespace tfx::flight
