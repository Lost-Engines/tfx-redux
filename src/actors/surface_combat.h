#pragma once

#include <optional>
#include <span>
#include "actors/air.h"
#include "actors/surface.h"
#include "flight/aircraft.h"
#include "weapons/runtime.h"

namespace tfx::actors {

struct surface_air_target {
  unsigned int actor;
  vec3<int32_t> position;
  uint16_t flags;
};
struct surface_combat_context {
  unsigned int owner;
  vec3<int32_t> player;
  flight::player_aircraft aircraft;
  std::span<surface_air_target const> aircraft_targets;
};
std::vector<weapons::release> engage_surface_targets(surface_state &state, profile const &profile, surface_combat_context const &context, math::random_state &random);
std::optional<weapons::release> surface_ground_attack(surface_state const &state, unsigned int owner, surface_request const &request, bool intact);

} // namespace tfx::actors
