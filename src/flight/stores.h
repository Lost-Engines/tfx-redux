#pragma once

#include <array>
#include <cstdint>
#include "vectorstorm/vector/vector2.h"
#include "vectorstorm/vector/vector3.h"
#include "flight/aircraft.h"

namespace tfx::flight {

unsigned int constexpr store_slots{10};
struct store_properties {
  int16_t mass_kg{0};
  float drag{0};
  bool fuel_tank{false};
};
struct mounted_store {
  vec3d position{0, 0, 0};
  store_properties properties{};
  bool occupied{false};
};
using loadout = std::array<mounted_store, store_slots>;
struct airframe_load {
  player_aircraft aircraft{player_aircraft::eurofighter};
  double moment_a{0};
  double moment_b{0};
  double fuel_capacity{0};
};
struct store_effects {
  double mass_kg{0};
  double drag{0};
  double moment_a{0};
  double moment_b{0};
  double total_moment_a{0};
  double total_moment_b{0};
  double fuel_capacity{0};
  bool fuel_tank{false};
};

store_effects accumulate_stores(airframe_load const &airframe, loadout const &stores);

} // namespace tfx::flight
