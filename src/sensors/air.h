#pragma once

#include <functional>
#include "actors/air.h"
#include "weapons/definition.h"

namespace tfx::sensors {

unsigned int constexpr contact_capacity{20};
struct air_contact {
  unsigned int actor;
  vec3<int32_t> position;                                                      // Native Q8 actor coordinates.
  math::angle heading;
  uint16_t flags;
  actors::allegiance side{actors::allegiance::enemy};
  bool enemy_pool{false};                                                      // The warning scan distinguishes the actor registry, not the model classification.
};
struct air_scan_report {
  bool more_hostiles{false};
  std::optional<unsigned int> announced_actor{};
};
class air_contacts {
public:
  std::array<std::optional<unsigned int>,contact_capacity> slots{};
  int16_t cursor{-1};
  std::optional<unsigned int> selected{};
  bool hostile_nearby{false};                                                  // Within 50 miles.
  int16_t nearby_hostile_count{0};                                             // Within 25 miles; retained between missions.
  air_scan_report update(vec3<int32_t> const &player, std::span<air_contact const> contacts);
  void next();
  unsigned int count() const;
};
struct air_lock {
  std::optional<unsigned int> tracking{};
  std::optional<unsigned int> guidance{};
  vec2<int32_t> reticle{0,0};
  vec2<int16_t> projected{0,0};
  int16_t lock_ticks{0};
  int16_t acquisition_ticks{0};
  bool too_close{false};
  void begin_mission(vec2<int16_t> const &projection_origin);
  void clear(vec2<int16_t> const &centre = {160,100});
};
struct lock_context {
  vec3<int32_t> position;                                                      // Player's world-integer pose, as used by the native HUD.
  vec3<math::angle> angles;
  simulation::timer_ticks elapsed;
  bool guns{false};
  bool sensor_suppressed{false};
  vec2<int16_t> centre{160,100};
  std::function<vec2<int16_t>(vec3<int32_t> const&)> project;
};
void advance_air_lock(air_lock &state, air_contacts &radar, std::span<air_contact const> contacts, weapons::definition const &weapon, lock_context const &context);

} // namespace tfx::sensors
