#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "math/native.h"
#include "resources/binary.h"

namespace tfx::actors {

unsigned int constexpr air_profile_bytes{74};
unsigned int constexpr aircraft_per_group{10}, aircraft_per_side{50}, surface_capacity{50};
int constexpr position_fraction_bits{8}, speed_fraction_bits{16};
int constexpr position_unit{1 << position_fraction_bits};
int constexpr minimum_world_y{-16384}, maximum_world_y{0};
enum class record_kind : uint8_t {
  aircraft = 14,
  reserved,
  surface,
  scenery,
  player_route
};
enum class profile_kind : uint8_t {
  aircraft = 14,
  type15,
  surface,
  type17
};
enum class allegiance : uint8_t {
  enemy,
  allied,
  neutral
};
enum class command : uint8_t {
  end,
  waypoint_branch,
  move_until_near,
  wait,
  takeoff,
  conditional_action,
  radio_message,
  reserved_7,
  reserved_8,
  pursue_waypoint,
  attack_ground,
  altitude,
  conditional_action_12,
  speed,
  aircraft_initial_state,
  reserved_15,
  surface_initial_state,
};
struct instruction {
  unsigned int address;
  command opcode;
  std::vector<uint8_t> operands;
};
struct actor_record {
  record_kind kind;
  unsigned int source_offset;
  resources::bytes body;
  std::vector<instruction> commands;
};
struct program {
  std::vector<actor_record> records;
  std::vector<vec3<int32_t>> player_route;
  explicit program(std::span<uint8_t const> source);
};
struct profile {
  uint16_t id;
  profile_kind kind;
  allegiance side;
  std::string name;
  std::string definition;
  resources::bytes source;
  resources::bytes native_window{};
};
std::vector<profile> profiles(std::span<uint8_t const> source);
struct formation_offset {
  math::angle heading;
  int32_t distance;
  int32_t height;
  int32_t forward_distance{0};
};
struct initial_actor {
  record_kind kind;
  unsigned int record;
  uint16_t profile_id;
  unsigned int slot;
  allegiance side;
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int32_t speed;
  uint16_t flags;                                                              // Native air word or surface byte; use the corresponding actor-kind enum.
  uint8_t leader{255};
  uint8_t formation{0};
};
std::vector<initial_actor> initialise(program const &source, std::span<profile const> profiles, std::span<formation_offset const> formation);
void move(vec3<int32_t> &position, vec3<math::angle> const &angles, int32_t distance, int minimum_y = minimum_world_y, int maximum_y = maximum_world_y);

} // namespace tfx::actors
