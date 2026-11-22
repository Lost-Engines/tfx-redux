#pragma once

#include <array>
#include "flight/stores.h"
#include "weapons/runtime.h"

namespace tfx::weapons {

unsigned int constexpr pylon_pairs{flight::store_slots / 2};
int16_t constexpr no_pylon{-1}, cannon_pylon{flight::store_slots};
uint8_t constexpr empty_pylon{255};
struct arming_tables {
  std::array<std::vector<std::array<int16_t, pylon_pairs>>, 3> defaults;
  std::array<uint16_t, flight::store_slots + 1> next;
  // The DOS selector also reads these fields for the empty-slot byte (255), outside the 27-record weapon table.
  weapon_role empty_role{weapon_role::air};
  uint8_t empty_category{0};
};
struct pylon {
  uint8_t weapon{empty_pylon};
  uint8_t count{0};
  bool visible{false};
};
using pylon_inventory = std::array<pylon, flight::store_slots + 1>;
struct selection_change {
  bool clear_contact{false};
  bool clear_tracking{false};
};
struct store_context {
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int32_t speed;
  target_reference target{};
  bool inhibited{false};
  bool ejected{false};
  bool protected_player{false};
  bool unlimited{false};
  bool jettisoned{false};
};

class armament {
private:
  flight::player_aircraft aircraft;
  std::span<definition const> definitions;
  arming_tables const &tables;
  weapon_role cannon_role{weapon_role::air};
  weapon_role role(unsigned int slot) const;
  uint8_t category(unsigned int slot) const;

public:
  pylon_inventory pylons{};                                                    // Last entry is the native cannon selection sentinel.
  int16_t selected{0};
  int16_t last_released{no_pylon};
  bool guns{true};
  armament(flight::player_aircraft aircraft, std::span<definition const> definitions, arming_tables const &tables);
  weapon_role selected_role() const;
  flight::loadout flight_stores(std::span<vec3d const, flight::store_slots> mounts) const;
  bool can_fit(unsigned int slot, weapon_id weapon) const;
  void assign(unsigned int slot, std::optional<weapon_id> weapon);
  void fit_default(unsigned int index);
  selection_change select(weapon_role requested, bool different_type);
  unsigned int jettison(store_context const &context, std::span<vec3<int16_t> const> mounts, selection_change &selection, std::function<void(release const &)> const &emit = {});
  std::optional<release> fire(store_context const &context, std::span<vec3<int16_t> const> mounts, selection_change &selection, std::function<void(release const &)> const &emit = {});
};

} // namespace tfx::weapons
