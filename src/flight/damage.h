#pragma once

#include <functional>
#include "math/native.h"
#include "simulation/timing.h"

namespace tfx::flight {

// Slot names follow the original HUD labels and manual; consumers retain their actual native indices.
enum class aircraft_system : unsigned int {
  communications,
  flaps,
  left_engine,
  hud,
  wheel_brakes,
  right_engine,
  radar,
  air_brake,
  fuel,
  weapons,
  landing_gear,
  gun
};
enum class system_condition : uint8_t {
  unavailable,
  active,
  normal,
  failed = 11,
  intermittent = 14
};
unsigned int constexpr aircraft_system_count{12};
struct damage_state {
  std::array<system_condition, aircraft_system_count> systems;
  std::array<uint16_t, aircraft_system_count> repair_ticks{};
  double accumulated{0};
  int16_t right_display_selection{0};
  damage_state();
  system_condition condition(aircraft_system system) const;
};
void apply_damage(damage_state &state, int32_t severity, math::random_state &random, bool invulnerable = false, std::function<void()> const &notify = {});
void repair_systems(damage_state &state, simulation::timer_ticks elapsed);

} // namespace tfx::flight
