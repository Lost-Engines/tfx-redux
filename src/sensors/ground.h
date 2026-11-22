#pragma once

#include <functional>
#include "actors/surface.h"
#include "flight/aircraft.h"

namespace tfx::sensors {

// Native 0x93128 locks the mission cruise destination immediately, independently
// of laser designation, aircraft radar, stealth and the projection range.
struct cruise_target {
  vec3<int32_t> position_q8;
  vec2<int32_t> reticle_q8;
  vec2<int16_t> projected;
};
cruise_target acquire_cruise_target(vec3<int32_t> const &player, vec2<int32_t> const &destination_q8,
  std::function<vec2<int16_t>(vec3<int32_t> const &)> const &project);

struct surface_contact {
  unsigned int slot;
  vec3<int32_t> position;
  math::angle heading;
  actors::allegiance side;
  uint8_t flags;
  int16_t type;
  std::vector<vec3<int16_t>> damage_centres{};
};
struct designator_controls {
  bool up{false};
  bool down{false};
  bool left{false};
  bool right{false};
};
struct designator {
  math::angle pitch{0x500};
  math::angle heading{0};
  uint8_t zoom{0};
  vec3<int32_t> camera{0, 0, 0};
  vec3<int32_t> point{0, 0, 0};
};
vec3<int32_t> ground_intersection(vec3<int32_t> const &player, math::angle pitch, math::angle heading);
void aim_designator(designator &state, vec3<int32_t> const &player, vec3<math::angle> const &angles, flight::player_aircraft aircraft, designator_controls const &controls);
void follow_designation(designator &state, vec3<int32_t> const &player, vec3<int32_t> const &target);
struct mobile_designation {
  unsigned int slot;
  vec3<int16_t> offset;
};
vec3<int32_t> mobile_target_point(vec3<int32_t> const &position, vec3<int16_t> const &offset);
std::optional<mobile_designation> nearest_mobile_target(vec3<int32_t> const &point, std::span<surface_contact const> contacts, int32_t range = 10000, int16_t type = 55);
std::optional<unsigned int> next_radiation_target(int16_t &cursor, std::span<surface_contact const> contacts, int16_t type = 56, actors::allegiance side = actors::allegiance::enemy);
std::optional<unsigned int> nearest_ship(vec3<int32_t> const &player, std::span<surface_contact const> contacts, std::span<int16_t const> allowed_types);

} // namespace tfx::sensors
