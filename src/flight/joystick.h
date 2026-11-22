#pragma once

#include "math/native.h"
#include "simulation/timing.h"

namespace tfx::flight {

// Normalized digital-device axes enter after the DOS resistance calibration.
// Retain its signed-square response, JOYSCALE, word conversion and +/-512 clamp.
vec2<int16_t> joystick_axes(vec2f const &normalised, float scale);

enum class joystick_action {
  none,
  fire,
  ground_weapon,
  air_weapon,
  cycle_target,
  front_view,
  left_view,
  right_view,
  rear_view
};
struct joystick_buttons {
  // The native dispatcher decrements both profile tables, even when one is idle.
  std::array<std::array<int16_t, 16>, 2> remaining{};
  joystick_action advance(uint8_t pressed, uint8_t stick_type, simulation::timer_ticks elapsed);
};

} // namespace tfx::flight
