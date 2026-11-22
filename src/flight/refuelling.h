#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>
#include "actors/program.h"
#include "flight/aircraft.h"
#include "resources/binary.h"
#include "simulation/timing.h"

namespace tfx::flight {

struct refuel_ports {
  vec3<int16_t> player;
  vec3<int16_t> tanker;
};
struct refuel_assets {
  std::array<refuel_ports, 3> ports;
  double fuel_per_update;
  std::string afterburner_message;
  explicit refuel_assets(std::span<uint8_t const> executable);
};
struct refuel_aircraft {
  vec3<int32_t> position_q8{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
  uint16_t flags{0};
  int16_t shape_type{-1};
};
using refuel_registry = std::array<std::optional<refuel_aircraft>, actors::aircraft_per_side>;
struct refuel_player {
  player_aircraft aircraft{player_aircraft::eurofighter};
  vec3<int32_t> position{0, 0, 0};
  vec3<int32_t> position_q8{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
  int32_t speed{0};
};
inline vec3<int16_t> constexpr default_refuel_divisors{24, 12, 24};
inline vec3<int16_t> constexpr released_refuel_divisors{50, 30, 50};

struct refuel_state {
  bool enabled{false};
  uint8_t saved_weapon_mode{0};
  std::optional<unsigned int> selected{};
  vec3<int16_t> divisors{default_refuel_divisors};
  int16_t delay{0};
  uint8_t phase{0};                                                            // Native 0x13b04; the connection controller produces 0 or 3.
  int32_t horizontal_distance{0};
  vec3<int32_t> angle_errors{0, 0, 0};
};
enum class refuel_event_kind {
  urgent_sample,
  normal_sample,
  player_model,
  tanker_model,
  connected_voice
};
struct refuel_event {
  refuel_event_kind kind;
  unsigned int argument{0};
  std::optional<unsigned int> tanker{};
};
struct refuel_update {
  std::vector<refuel_event> events{};
  std::optional<unsigned int> target_override{};
  std::optional<unsigned int> moved_tanker{};
};
struct refuel_toggle {
  uint8_t weapon_mode;
  std::vector<refuel_event> events;
};
struct refuel_transfer {
  double fuel;
  bool afterburner_warning;
};
class refuelling {
private:
  refuel_assets const &assets;
  refuel_state state;
  bool aligned(refuel_player &player, refuel_aircraft const &tanker);
  bool follow(refuel_player const &player, refuel_aircraft &tanker, simulation::timer_ticks elapsed);

public:
  explicit refuelling(refuel_assets const &assets, refuel_state initial = {});
  refuel_state const &value() const;
  void reset();
  void disengage();                                                            // Ejection clears enable without restoring the previous weapon mode.
  refuel_toggle toggle(uint8_t weapon_mode, bool inhibited = false, bool crashed = false, bool ejected = false);
  refuel_update advance(refuel_player &player, refuel_registry &allies, simulation::timer_ticks elapsed);
  refuel_transfer transfer(double fuel, double capacity, bool afterburner) const;
};

} // namespace tfx::flight
