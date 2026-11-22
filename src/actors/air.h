#pragma once

#include <array>
#include <functional>
#include <optional>
#include <variant>
#include "actors/air_motion.h"
#include "actors/manoeuvre.h"
#include "flight/aircraft.h"

namespace tfx::actors {

enum class air_action : uint8_t {
  route,
  retire_ejected,
  retire_crashed,
  falling,
  normal,
  finish_takeoff,
  manoeuvre,
  engagement,
  withdraw,
  evasive,
  align_manoeuvre,
  waiting,
  clear
};

struct air_location {
  unsigned int record;
  unsigned int address;
  bool operator==(air_location const &) const = default;
};
unsigned int constexpr player_air_target{aircraft_per_side * 2};
struct air_contact {
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int32_t speed;
};
struct air_threat {
  vec3<int32_t> position;
  uint8_t weapon;
};
struct air_combat_profile {
  uint16_t maximum_player_shots;
  uint16_t firing_delay;
  uint16_t firing_cooldown;
  uint16_t gun_range;
  uint16_t evasion_range;
  uint16_t engagement_range;
};
struct air_actor {
  air_state flight;
  air_performance performance;
  air_location location;
  uint32_t actions{0};
  int16_t wait_seconds{0};
  air_combat_profile combat{};
  std::optional<unsigned int> target{};
  std::optional<manoeuvre_cursor> manoeuvre{};
  uint8_t scan_cursor{0};
  uint8_t player_shots{0};
};
struct air_target_scan {};
struct air_scenery_event {
  unsigned int entry;
};
struct air_radio {
  std::string text;
};
struct air_weapon {
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int16_t speed;
  uint16_t weapon;
  vec3<int32_t> target;
  std::optional<unsigned int> target_actor{};
};
struct air_impact {
  vec3<int32_t> position;
  unsigned int kind;
};
enum class air_loss_cause {
  combat,
  collision
};
enum class air_warning_kind {
  tracking,
  firing
};
struct air_warning {
  unsigned int target;
  air_warning_kind kind;
};
enum class air_countermeasure_kind {
  infrared,
  radar
};
struct air_countermeasure {
  air_countermeasure_kind kind;
  int16_t strength;
};
using air_request_data = std::variant<air_target_scan, air_scenery_event, air_radio, air_weapon, air_impact, air_warning, air_countermeasure>;
struct air_request {
  unsigned int actor;
  air_request_data data;
};
struct air_context {
  simulation::flight_model model{simulation::flight_model::arcade};
  std::function<bool(vec2<int16_t> const &)> intact_ground_target{};
  std::function<unsigned int(vec3<int32_t> const &)> impact_effect{};
  bool combat{false};
  bool weapons_enabled{true};
  std::optional<air_contact> player{};
  flight::player_aircraft player_aircraft{flight::player_aircraft::eurofighter};
  std::span<std::optional<air_threat> const> threats{};
  uint16_t combat_flags{0};
  std::function<void(air_request const &)> emitted{};
};

unsigned int air_registry_index(initial_actor const &actor);

class air_runtime {
private:
  class request_queue;
  std::vector<resources::bytes> commands;
  std::array<std::optional<air_actor>, aircraft_per_side * 2> actors;
  std::span<formation_offset const> formation;
  std::span<uint8_t const> envelope;
  math::random_state random;
  std::span<manoeuvre const> manoeuvres;
  std::array<std::vector<unsigned int>, 3> manoeuvre_banks;
  air_contact contact(unsigned int target, air_context const &context) const;
  bool acquire(unsigned int index, std::optional<unsigned int> excluded, air_context const &context);
  void select_manoeuvre(air_actor &actor, unsigned int category);
  void countermeasures(unsigned int index, air_context const &context, request_queue &requests);
  bool combat_action(unsigned int index, unsigned int action, air_context const &context, request_queue &requests);
  void course(air_actor &actor, vec3<int32_t> const &destination, int32_t distance, bool pitch, air_control_context const &context);
  bool follow_formation(air_actor &actor, air_control_context const &context);
  vec3<int32_t> formation_position(vec3<int32_t> const &position, math::angle heading, unsigned int member) const;
  void advance_actor(unsigned int index, air_context const &context, request_queue &requests);

public:
  air_runtime(program const &program, std::span<initial_actor const> initial, std::span<profile const> profiles, std::span<formation_offset const> formation, std::span<uint8_t const> envelope, math::random_state random = {}, std::span<manoeuvre const> manoeuvres = {});
  math::random_state &random_stream();
  std::vector<air_request> fatal_damage(unsigned int index, air_loss_cause cause = air_loss_cause::combat);
  std::span<std::optional<air_actor> const> aircraft() const;
  void apply_refuelling_pose(unsigned int index, vec3<int32_t> const &position_q8, vec3<math::angle> const &angles);
  std::span<uint8_t const> program_bytes(unsigned int record) const;
  std::vector<air_request> advance(air_context const &context = {});
};

} // namespace tfx::actors
