#pragma once

#include <functional>
#include <optional>
#include <span>
#include "actors/program.h"
#include "weapons/definition.h"

namespace tfx::weapons {

struct projectile {
  uint16_t next_stage{0};
  flight_stage motion{flight_stage::finished};
  int16_t ticks{0};
  vec3<int32_t> position;
  vec3<int32_t> previous;
  vec3<math::angle> angles;
  int32_t speed;
  int16_t fall_speed{0};
  int16_t acceleration{25};
  int32_t range;
  uint8_t weapon;
  bool secondary{false};
  std::span<stage const> program{};                                            // Optional borrowed stage sequence, owned alongside the weapon definitions.
};
struct motion_context {
  uint16_t elapsed;
  vec3<int32_t> target;
  bool player_owned{false};
  vec3<int32_t> launch_anchor{0,0,0};
  vec3<math::angle> player_angles{0,0,0};
  int32_t player_speed{0};
  bool target_is_player{false};
  bool player_ejected{false};
  bool player_decoys{false};
  int32_t player_distance{0};
  vec2<int32_t> height_bounds{actors::minimum_world_y,actors::maximum_world_y};
};
struct child_release {
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int32_t speed;
  unsigned int weapon;
};
struct motion_result {
  bool reset_visual{false};
  std::vector<child_release> children{};
};

struct motion_hooks {
  std::function<void(child_release const&)> release{};
  std::function<definition const&(uint8_t)> current_definition{};
};

// Native 0xa373c excludes these three callbacks by identity, including after ejection.
bool has_swept_contacts(flight_stage motion);
// Post-motion trail gate; world/weapon/detail flags have separate owners.
bool has_smoke_trail(flight_stage motion, int32_t player_distance);
void next_stage(projectile &state, definition const &weapon);
projectile launch(definition const &weapon, uint8_t index, vec3<int32_t> const &position, vec3<math::angle> const &angles, int32_t speed, std::span<stage const> program = {});
motion_result advance_motion(projectile &state, definition const &weapon, motion_context const &context, math::random_state &random, motion_hooks const &hooks = {});

} // namespace tfx::weapons
