#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "world/environment.h"

namespace tfx::missions {

using script_time = int32_t;
script_time constexpr script_ticks_per_second{54};
unsigned int constexpr maximum_targets{9}, maximum_flak_regions{5}, maximum_messages{10}, maximum_scenery_events{16};

struct inherited_setup {
  vec3<int32_t> position{0, 0, 0};
  float speed_factor{1};
  uint8_t weapons{0};
};
struct target {
  vec3<int32_t> position;
  script_time time_limit{-1};
  std::optional<std::string> text{};
};
enum class completion_kind : uint8_t {
  enemy,
  allied,
  all_targets,
  timer,
  enemy_mobile,
  protect
};
struct completion_rule {
  completion_kind kind;
  int32_t value;
  std::optional<std::string> text{};
};
struct flak_region {
  vec2<int32_t> first;
  vec2<int32_t> last;
};
struct timed_message {
  script_time time;
  std::string text;
};
struct scenery_event {
  script_time time;
  uint8_t entry;
  vec2<uint8_t> cell;
};
struct setup {
  std::string world{};
  std::string environment{};
  std::string waypoints{};
  world::mission_start start;
  world::player_aircraft aircraft{world::player_aircraft::eurofighter};
  vec3<math::angle> angles{0, 0, 0};
  float speed_factor;
  uint8_t weapons;
  bool default_weapons{false};
  std::optional<std::string> saved_start_extension{};
  int16_t aircraft_type_filter{-1};
  int16_t surface_type_filter{-1};
  std::optional<int32_t> timer{};
  script_time clock{0};
  std::vector<target> targets{};
  std::optional<completion_rule> completion{};
  std::vector<flak_region> flak{};
  std::vector<timed_message> messages{};
  std::vector<scenery_event> scenery_events{};
};

setup initialise(std::string_view source, inherited_setup const &inherited = {});

} // namespace tfx::missions
