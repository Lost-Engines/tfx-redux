#include "air.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace tfx::sensors {
namespace {

int16_t constexpr initial_lock_ticks{500};

int32_t subtract(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? subtract(0, value) : value;
}
int16_t word(int32_t const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
vec3<int32_t> integer_position(air_contact const &contact) {
  return {contact.position.x >> actors::position_fraction_bits, contact.position.y >> actors::position_fraction_bits, contact.position.z >> actors::position_fraction_bits};
}

} // anonymous namespace

air_scan_report air_contacts::update(vec3<int32_t> const &player, std::span<air_contact const> const contacts) {
  // Native 0x836ac/0x8296c preserves slots across successive scans; new contacts fill the remaining holes.
  auto const previous{slots};
  slots.fill({});
  hostile_nearby = false;
  std::vector<unsigned int> added;
  int16_t nearby_count{0};
  int16_t farthest{0};
  std::optional<unsigned int> announcement;
  int constexpr announcement_miles{25};
  for(auto const &contact : contacts) {
    if(contact.flags & std::to_underlying(actors::air_flag::retired)) continue;
    auto const position{integer_position(contact)};
    int constexpr world_units_per_mile{660};
    int constexpr radar_miles{50};
    auto const x{word(magnitude(word(subtract(position.x, player.x) / world_units_per_mile)))};
    auto const z{word(magnitude(word(subtract(player.z, position.z) / world_units_per_mile)))};
    if(std::max(x, z) >= radar_miles) continue;
    if(contact.enemy_pool) {
      hostile_nearby = true;
      auto const distance{std::max(x, z)};
      if(distance < announcement_miles) {
        ++nearby_count;
        if(distance > farthest) {
          farthest = distance;
          announcement = contact.actor;
        }
      }
    }
    auto const found{std::ranges::find(previous, contact.actor)};
    if(found != previous.end()) {
      slots[static_cast<unsigned int>(found - previous.begin())] = contact.actor;
    } else {
      added.push_back(contact.actor);
    }
  }
  for(auto const actor : added) {
    auto const free{std::ranges::find(slots, std::optional<unsigned int>{})};
    if(free == slots.end()) break;
    *free = actor;
  }
  air_scan_report const report{
    .more_hostiles{nearby_count > nearby_hostile_count},
    .announced_actor{nearby_count > nearby_hostile_count ? announcement : std::nullopt},
  };
  nearby_hostile_count = nearby_count;
  return report;
}

void air_contacts::next() {
  // 0x828b0: with no previous cursor the search starts at zero, otherwise immediately after the old slot.
  bool const initial{cursor == -1};
  unsigned int const start{initial ? 0u : static_cast<unsigned int>(cursor)};
  unsigned int index{initial ? 0u : (start + 1) % contact_capacity};
  while(!slots[index] && index != start) index = (index + 1) % contact_capacity;
  if(slots[index]) {
    selected = slots[index];
    cursor = static_cast<int16_t>(index);
  } else {
    cursor = -1;
    if(!initial) selected.reset();
  }
}

unsigned int air_contacts::count() const {
  return static_cast<unsigned int>(std::ranges::count_if(slots, [](auto const &slot){return slot.has_value();}));
}

void air_lock::begin_mission(vec2<int16_t> const &projection_origin) {
  // Native 0x930d4 leaves the projected target and too-close flag alone.
  // Its reticle starts at the renderer's inherited preflight aperture centre.
  tracking.reset();
  guidance.reset();
  reticle = {static_cast<int32_t>(projection_origin.x) * 256, static_cast<int32_t>(projection_origin.y) * 256};
  lock_ticks = initial_lock_ticks;
  acquisition_ticks = 0;
}

void air_lock::clear(vec2<int16_t> const &centre) {
  guidance.reset();
  tracking.reset();
  reticle = {static_cast<int32_t>(centre.x) * 256, static_cast<int32_t>(centre.y) * 256};
  projected = {0, 0};
  too_close = false;
}

void advance_air_lock(air_lock &state, air_contacts &radar, std::span<air_contact const> const contacts, weapons::definition const &weapon, lock_context const &context) {
  // 0x93328 has separate controllers for laser, anti-radiation, cruise and anti-ship weapons.
  if(weapon.category == 3 || weapon.category == 8 || weapon.category == weapons::cruise_missile_category || weapon.category == 12 || weapon.category == 13 || context.sensor_suppressed) return;
  if(!radar.selected) radar.next();
  if(!radar.selected) return;
  if(!radar.count()) {
    state.guidance.reset();
    state.tracking.reset();
    radar.selected.reset();
    return;
  }
  auto const found{std::ranges::find(contacts, *radar.selected, &air_contact::actor)};
  if(found == contacts.end()) {
    state.clear(context.centre);
    radar.selected.reset();
    return;
  }
  auto const position{integer_position(*found)};
  auto const offset{vec3<int32_t>{magnitude(subtract(position.x, context.position.x)), magnitude(subtract(position.y, context.position.y)), magnitude(subtract(position.z, context.position.z))}};
  state.too_close = math::distance_estimate(position, context.position) < weapon.minimum_range;
  auto const direction{math::bearing(context.position, position)};
  int constexpr acquisition_cone{0x1200};
  int constexpr rear_aspect{0x2000};
  if(weapon.category == 0 && magnitude(static_cast<int32_t>(math::signed_word(context.angles.y)) - math::signed_word(found->heading)) > rear_aspect) {
    state.clear(context.centre);
  } else {
    state.tracking = found->actor;
  }
  if(magnitude(static_cast<int32_t>(math::signed_word(direction.x)) - math::signed_word(context.angles.x)) > acquisition_cone || magnitude(static_cast<int32_t>(math::signed_word(direction.y)) - math::signed_word(context.angles.y)) > acquisition_cone) state.clear(context.centre);
  // Native range selection compares signed low words before comparing the resulting signed-word distance to range.
  int32_t const chosen{word(offset.x) > word(offset.z) ? offset.x : offset.z};
  if(word(chosen) > weapon.range || state.too_close) state.clear(context.centre);
  if(std::max(offset.x, offset.z) > 65535) {
    state.clear(context.centre);
    return;
  }
  if(weapon.category == 14 || (found->side != actors::allegiance::enemy && found->side != actors::allegiance::neutral) || context.guns || weapon.role == weapons::weapon_role::ground) state.clear(context.centre);
  state.projected = context.project(position);
  if(state.guidance) {
    state.guidance = found->actor;
    state.reticle = {static_cast<int32_t>(state.projected.x) * 256, static_cast<int32_t>(state.projected.y) * 256};
    if(state.lock_ticks) state.lock_ticks = static_cast<int16_t>(std::max(0, static_cast<int>(word(state.lock_ticks - context.elapsed))));
    return;
  }
  if(!state.tracking) return;
  auto const difference{vec2<int32_t>{static_cast<int32_t>(state.projected.x) - (state.reticle.x >> 8), static_cast<int32_t>(state.projected.y) - (state.reticle.y >> 8)}};
  uint32_t const square{static_cast<uint32_t>(difference.x) * static_cast<uint32_t>(difference.x) + static_cast<uint32_t>(difference.y) * static_cast<uint32_t>(difference.y)};
  uint16_t const distance{static_cast<uint16_t>(std::sqrt(static_cast<double>(square)))};
  state.lock_ticks = initial_lock_ticks;
  int32_t const movement{static_cast<int32_t>(context.elapsed) * 15 * 256 / 50};
  if(static_cast<int32_t>(distance) * 256 <= movement) {
    state.guidance = found->actor;
    return;
  }
  for(unsigned int axis{0}; axis != 2; ++axis) {
    int32_t const product{std::bit_cast<int32_t>(static_cast<uint32_t>(difference[axis]) * static_cast<uint32_t>(movement))};
    state.reticle[axis] = std::bit_cast<int32_t>(static_cast<uint32_t>(state.reticle[axis]) + static_cast<uint32_t>(product / distance));
  }
  state.acquisition_ticks = word(state.acquisition_ticks - context.elapsed);
  if(state.acquisition_ticks < 0) state.acquisition_ticks = static_cast<int16_t>(distance / 4);
}

} // namespace tfx::sensors
