#include "loadout.h"
#include <stdexcept>
#include <utility>

namespace tfx::weapons {

armament::armament(flight::player_aircraft const aircraft, std::span<definition const> const definitions, arming_tables const &tables)
  : aircraft{aircraft},
    definitions{definitions},
    tables{tables} {
  if(definitions.size() != weapon_count) throw std::runtime_error{"arming requires the complete weapon definitions"};
  cannon_role = definitions[0].role;
  pylons[cannon_pylon].weapon = std::to_underlying(weapon_id::cannon);
  // Verify the authored successor table is one complete ring, including the cannon sentinel.
  unsigned int cursor{0};
  std::array<bool, flight::store_slots + 1> seen{};
  for(unsigned int count{0}; count != seen.size(); ++count) {
    if(cursor >= seen.size() || seen[cursor]) throw std::runtime_error{"invalid pylon selection ring"};
    seen[cursor] = true;
    cursor = tables.next[cursor];
  }
  if(cursor != 0) throw std::runtime_error{"unterminated pylon selection ring"};
}

weapon_role armament::selected_role() const {
  return selected < 0 ? cannon_role : role(static_cast<unsigned int>(selected));
}

flight::loadout armament::flight_stores(std::span<vec3d const, flight::store_slots> const mounts) const {
  flight::loadout result;
  for(unsigned int slot{0}; slot != result.size(); ++slot) {
    auto const &pylon{pylons[slot]};
    if(!pylon.count || pylon.weapon == empty_pylon) continue;
    if(pylon.weapon >= definitions.size()) throw std::out_of_range{"flight stores require a valid weapon definition"};
    auto const &weapon{definitions[pylon.weapon]};
    result[slot] = {
      .position{vec3d{mounts[slot]}},
      .properties{
        .mass_kg{weapon.mass},
        .drag{weapon.drag},
        .fuel_tank{weapon.role == weapon_role::external_tank},
      },
      .occupied{true},
    };
  }
  return result;
}

bool armament::can_fit(unsigned int const slot, weapon_id const weapon) const {
  if(slot >= flight::store_slots) throw std::out_of_range{"mount permission requires a store slot"};
  auto const candidate{std::to_underlying(weapon)};
  if(candidate >= definitions.size()) throw std::out_of_range{"mount permission requires a weapon definition"};
  auto const mask{definitions[candidate].mount_masks.at(std::to_underlying(aircraft) - 1)};
  if(!(mask & (1u << slot))) return false;
  if(aircraft == flight::player_aircraft::eurofighter) return true;
  // Native 0x89c08 shares these mutually exclusive attachment positions in
  // F-117 and F-22. Model presence (including a temporary preview) matters,
  // independently of the carried quantity or weapon byte.
  uint16_t constexpr blocked_by_slots_0_and_7{(1u << 3) | (1u << 4) | (1u << 8) | (1u << 9)};
  uint16_t constexpr blocked_by_slots_3_4_8_9{(1u << 0) | (1u << 7)};
  std::array<uint16_t, flight::store_slots> constexpr conflicts{
    blocked_by_slots_0_and_7, 0, 0, blocked_by_slots_3_4_8_9, blocked_by_slots_3_4_8_9,
    0, 0, blocked_by_slots_0_and_7, blocked_by_slots_3_4_8_9, blocked_by_slots_3_4_8_9,
  };
  for(unsigned int other{0}; other != flight::store_slots; ++other) {
    if((conflicts[slot] & (1u << other)) && pylons[other].visible) return false;
  }
  return true;
}
void armament::assign(unsigned int const slot, std::optional<weapon_id> const weapon) {
  if(slot >= flight::store_slots) throw std::out_of_range{"arming requires a store slot"};
  if(weapon && std::to_underlying(*weapon) >= definitions.size()) throw std::out_of_range{"arming requires a weapon definition"};
  // Native 0x8b3d4 is the setter; permission checks belong to its interactive caller.
  pylons[slot] = {
    .weapon{weapon ? std::to_underlying(*weapon) : empty_pylon},
    .count{static_cast<uint8_t>(weapon.has_value())},
    .visible{weapon.has_value()},
  };
}

void armament::fit_default(unsigned int const index) {
  auto const &fit{tables.defaults.at(std::to_underlying(aircraft) - 1).at(index)};
  std::array<std::array<unsigned int, 2>, pylon_pairs> constexpr pairs{{{0, 7}, {1, 6}, {2, 5}, {3, 4}, {8, 9}}};
  for(unsigned int pair{0}; pair != pairs.size(); ++pair) {
    auto const weapon{fit[pair]};
    if(weapon < -1 || weapon >= static_cast<int>(definitions.size())) throw std::runtime_error{"invalid default pylon weapon"};
    for(auto const slot : pairs[pair]) {
      pylons[slot] = {
        .weapon{static_cast<uint8_t>(weapon)},
        .count{static_cast<uint8_t>(weapon >= 0)},
        .visible{weapon >= 0},
      };
    }
  }
}

weapon_role armament::role(unsigned int const slot) const {
  if(pylons.at(slot).weapon == std::to_underlying(weapon_id::cannon)) return cannon_role;
  return pylons[slot].weapon == empty_pylon ? tables.empty_role : definitions[pylons[slot].weapon].role;
}

uint8_t armament::category(unsigned int const slot) const {
  return pylons.at(slot).weapon == empty_pylon ? tables.empty_category : definitions[pylons[slot].weapon].category;
}

selection_change armament::select(weapon_role const requested, bool const different_type) {
  // 0x8b9c4 skips another pylon with the same weapon; 0x8bba0 is the automatic post-release advance.
  selection_change result;
  if(selected == no_pylon) {
    return {
      .clear_contact{true},
      .clear_tracking{true}
    };
  }
  auto const original{static_cast<unsigned int>(selected)};
  unsigned int next{original};
  do {
    next = tables.next[next];
    if(next == original || (next == cannon_pylon && aircraft != flight::player_aircraft::nighthawk)) break;
    if(role(next) == requested && pylons[next].count && (!different_type || pylons[next].weapon != pylons[original].weapon)) break;
  } while(next != original);
  // Harpoon's separate designated contact is discarded even when the selected type is unchanged.
  if(category(original) == 13 || (category(next) != category(original) && role(next) == weapon_role::ground)) {
    result = {
      .clear_contact{true},
      .clear_tracking{true}
    };
  }
  if(next == cannon_pylon) {
    cannon_role = requested;
    guns = true;
  } else if(next == original || role(next) != requested || !pylons[next].count) {
    selected = no_pylon;
    result.clear_contact = true;
  }
  if(selected != no_pylon) selected = static_cast<int16_t>(next);
  if(selected == no_pylon || next == cannon_pylon || pylons[next].weapon != pylons[original].weapon) result.clear_tracking = true;
  return result;
}

unsigned int armament::jettison(store_context const &context, std::span<vec3<int16_t> const> const mounts, selection_change &selection, std::function<void(release const &)> const &emit) {
  // Native Alt+J (0x85118) visits each ground/tank pylon once, in slot order.
  if(context.inhibited || context.ejected || context.protected_player) return 0;
  auto dropped{context};
  dropped.jettisoned = true;
  unsigned int count{0};
  for(unsigned int slot{0}; slot != flight::store_slots; ++slot) {
    auto const purpose{role(slot)};
    if(!pylons[slot].count || (purpose != weapon_role::ground && purpose != weapon_role::external_tank)) continue;
    selected = static_cast<int16_t>(slot);
    selection_change change;
    if(fire(dropped, mounts, change, emit)) ++count;
    selection.clear_contact = selection.clear_contact || change.clear_contact;
    selection.clear_tracking = selection.clear_tracking || change.clear_tracking;
    // The post-release selector can discard the target before the next drop.
    if(change.clear_tracking) dropped.target = {.untargeted{true}};
  }
  return count;
}

std::optional<release> armament::fire(store_context const &context, std::span<vec3<int16_t> const> const mounts, selection_change &selection, std::function<void(release const &)> const &emit) {
  // Native 0x8bd8c is a release primitive; its caller performs trigger, landing and target-lock checks.
  if(context.inhibited || context.ejected || context.protected_player || selected == no_pylon) return {};
  if(selected == cannon_pylon || mounts.size() != flight::store_slots) throw std::runtime_error{"store release requires a carried pylon"};
  auto const slot{static_cast<unsigned int>(selected)};
  if(pylons[slot].weapon >= definitions.size()) throw std::runtime_error{"store release has no weapon"};
  auto const origin{vec3<int32_t>{vec3<uint32_t>{context.position} * static_cast<uint32_t>(actors::position_unit)}};
  release shot{
    .weapon{pylons[slot].weapon},
    .position{math::place_anchor(origin, context.angles, mounts[slot])},
    .angles{vec3<math::angle>{context.angles}},
    .speed{context.speed},
    .owner{
      .pool{actor_pool::player},
    },
    .target{context.target},
    .mount{static_cast<uint8_t>(slot)},
    .jettisoned{context.jettisoned},
  };
  if(emit) emit(shot);                                                         // Construction observes the current pylon before post-release selection changes it.
  last_released = selected;
  if(!context.unlimited) --pylons[slot].count;                                 // Byte underflow is retained: eligibility belongs to the caller.
  if(!pylons[slot].count && pylons[slot].weapon != std::to_underlying(weapon_id::jp233)) pylons[slot].visible = false;
  if(!context.unlimited) {
    selection = select(role(slot), false);
    if(selected == no_pylon) guns = true;
  }
  return shot;
}

} // namespace tfx::weapons
