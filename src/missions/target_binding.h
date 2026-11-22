#pragma once

#include <span>
#include <variant>
#include "actors/program.h"
#include "world/environment.h"

namespace tfx::missions {

struct fixed_target {
  unsigned int index;
};
struct surface_target {
  unsigned int slot;
};
struct terrain_target {
  vec2<int32_t> cell;
};
using target_binding = std::variant<fixed_target, surface_target, terrain_target>;

struct surface_target_candidate {
  unsigned int slot;
  vec3<int32_t> position_q8;
  uint8_t flags;
};

// Native 70B70 binds once at launch: fixed scenery in linked-list order,
// eligible surface slots in ascending order, then the terrain cell.
target_binding bind_target(vec3<int32_t> const &position,
  std::span<vec3<int32_t> const> fixed_positions_q8,
  std::span<surface_target_candidate const> surface);

} // namespace tfx::missions
