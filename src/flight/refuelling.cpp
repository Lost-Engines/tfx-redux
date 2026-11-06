#include "refuelling.h"
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "actors/air_motion.h"

namespace tfx::flight {
namespace {

int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }
int32_t add(int32_t const a, int32_t const b) { return wrap(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t subtract(int32_t const a, int32_t const b) { return wrap(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t magnitude(int32_t const value) { return value < 0 ? subtract(0,value) : value; }
int32_t angle_difference(math::angle const a, math::angle const b) {
  // Native subtraction sign-extends each word separately, without angular wrap.
  return magnitude(math::signed_word(a) - math::signed_word(b));
}
vec3<int32_t> q8(vec3<int32_t> const &position) {
  return {wrap(static_cast<uint32_t>(position.x) << actors::position_fraction_bits),wrap(static_cast<uint32_t>(position.y) << actors::position_fraction_bits),wrap(static_cast<uint32_t>(position.z) << actors::position_fraction_bits)};
}
std::optional<unsigned int> nearest(refuel_player const &player, refuel_registry const &allies) {
  // Native 0x759c0 scans allied slots forwards; strict comparison keeps the first tie.
  int16_t constexpr tanker_shape_type{50};
  uint32_t constexpr search_range{200000u << actors::position_fraction_bits};
  auto const origin{q8(player.position)};
  uint32_t distance{search_range};
  std::optional<unsigned int> selected;
  for(unsigned int index{0}; index != allies.size(); ++index) {
    auto const &candidate{allies[index]};
    if(!candidate || (candidate->flags & std::to_underlying(actors::air_flag::retired)) || candidate->shape_type != tanker_shape_type) continue;
    auto const next{static_cast<uint32_t>(math::distance_estimate(origin,candidate->position_q8))};
    if(next < distance) { distance = next;selected = index; }
  }
  return selected;
}

} // anonymous namespace

refuel_assets::refuel_assets(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr data{0xedaa4}, player_ports{0x8c64}, tanker_ports{0x8c70};
  auto const point{[&](unsigned int const table, unsigned int const index) {
    auto const address{data + source.le32(data + table + index * 4)};
    return vec3<int16_t>{source.signed_le16(address),source.signed_le16(address + 2),source.signed_le16(address + 4)};
  }};
  for(unsigned int craft{0}; craft != ports.size(); ++craft) ports[craft] = {
    .player{point(player_ports,craft)},
    .tanker{point(tanker_ports,craft)},
  };
  fuel_per_update = std::bit_cast<double>(static_cast<uint64_t>(source.le32(data + 0x2411)) | (static_cast<uint64_t>(source.le32(data + 0x2415)) << 32));
  for(unsigned int at{data + 0x23e4}; source.byte(at); ++at) afterburner_message += static_cast<char>(source.byte(at));
}
refuelling::refuelling(refuel_assets const &source, refuel_state initial) : assets{source}, state{std::move(initial)} {}
refuel_state const &refuelling::value() const { return state; }
void refuelling::reset() { state = {}; }
void refuelling::disengage() { state.enabled = false; }

refuel_toggle refuelling::toggle(uint8_t const weapon_mode, bool const inhibited, bool const crashed, bool const ejected) {
  refuel_toggle result{
    .weapon_mode{weapon_mode},
    .events{},
  };
  if(inhibited || crashed || ejected) return result;
  result.events.push_back({
    .kind{refuel_event_kind::normal_sample},
    .argument{8},
  });
  result.events.push_back({
    .kind{refuel_event_kind::normal_sample},
    .argument{0},
  });
  result.events.push_back({
    .kind{refuel_event_kind::player_model},
    .argument{state.enabled ? 8u : 7u},
  });
  state.delay = 0;
  if(state.enabled) result.weapon_mode = state.saved_weapon_mode;
  else { state.saved_weapon_mode = weapon_mode;result.weapon_mode = 0; }
  state.enabled = !state.enabled;
  return result;
}
bool refuelling::aligned(refuel_player &player, refuel_aircraft const &tanker) {
  auto const origin{q8(player.position)};
  uint32_t const x{static_cast<uint32_t>(origin.x) - static_cast<uint32_t>(tanker.position_q8.x)};
  uint32_t const z{static_cast<uint32_t>(origin.z) - static_cast<uint32_t>(tanker.position_q8.z)};
  // Native 0xcbeb8 squares/adds with dword wrapping, then returns the unsigned
  // word square root. This is deliberately not an unbounded Euclidean distance.
  state.horizontal_distance = static_cast<int32_t>(std::sqrt(static_cast<double>(x * x + z * z)));
  int32_t constexpr near_distance_q8{50000}, alignment_angle{0x2000};
  bool aligned{false};
  if(state.horizontal_distance < near_distance_q8) {
    auto const bearing{math::bearing(origin,tanker.position_q8)};
    state.angle_errors.x = angle_difference(bearing.x,player.angles.x);
    state.angle_errors.y = angle_difference(bearing.y,player.angles.y);
    aligned = state.angle_errors.x < alignment_angle && state.angle_errors.y < alignment_angle;
  }
  // Original 0x94e34 restores temporary Q8 globals with SAR, rather than a saved
  // copy. Normal world coordinates fit; retain that wrap at synthetic boundaries.
  for(unsigned int axis{0}; axis != 3; ++axis) player.position[axis] = origin[axis] >> actors::position_fraction_bits;
  return aligned;
}
bool refuelling::follow(refuel_player const &player, refuel_aircraft &tanker, simulation::timer_ticks const elapsed) {
  if(state.delay) state.delay = math::signed_word(static_cast<uint16_t>(state.delay - elapsed));
  int32_t constexpr minimum_speed{270}, maximum_speed{360};
  if(player.speed < minimum_speed || player.speed > maximum_speed || state.delay > 0) {
    state.phase = 0;
    state.divisors = vec3<int16_t>{default_refuel_divisors};
    return false;
  }
  state.delay = 0;
  auto const &ports{assets.ports.at(std::to_underlying(player.aircraft) - 1)};
  auto const player_port{math::place_anchor(player.position_q8,player.angles,ports.player)};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    int32_t const old{math::signed_word(tanker.angles[axis])};
    int32_t const delta{old - math::signed_word(player.angles[axis])};
    unsigned int constexpr angle_response_shift{4};
    tanker.angles[axis] = static_cast<math::angle>(old - (delta >> angle_response_shift));
  }
  auto const tanker_port{math::place_anchor(player_port,tanker.angles,ports.tanker)};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    if(!state.divisors[axis]) throw std::runtime_error{"zero native refuelling follow divisor"};
    int32_t const target{subtract(add(player_port[axis],player_port[axis]),tanker_port[axis])};
    int32_t const delta{subtract(tanker.position_q8[axis],target)};
    int32_t constexpr snap_tolerance_per_tick{8};
    if(magnitude(delta / state.divisors[axis]) < static_cast<int32_t>(elapsed) * snap_tolerance_per_tick) state.divisors[axis] = 1;
    tanker.position_q8[axis] = subtract(tanker.position_q8[axis],delta / state.divisors[axis]);
  }
  return true;
}
refuel_update refuelling::advance(refuel_player &player, refuel_registry &allies, simulation::timer_ticks const elapsed) {
  refuel_update result;
  auto const previous{state.selected};
  state.selected = state.enabled ? nearest(player,allies) : std::nullopt;
  if(!state.selected) {
    state.delay = 0;
    state.phase = 0;
    state.divisors = vec3<int16_t>{default_refuel_divisors};
    return result;
  }
  unsigned int const selected{*state.selected};
  auto &tanker{*allies[selected]};
  if(!previous) result.events.push_back({
    .kind{refuel_event_kind::urgent_sample},
    .argument{46},
  });
  result.target_override = selected;
  if(state.divisors.x == 1 && state.divisors.y == 1 && state.divisors.z == 1) {
    if(!state.phase) result.events.push_back({
      .kind{refuel_event_kind::connected_voice},
    });
    uint8_t constexpr coupled_phase{3};
    state.phase = coupled_phase;
    result.events.push_back({
      .kind{refuel_event_kind::tanker_model},
      .argument{19},
      .tanker{selected},
    });
    if(follow(player,tanker,elapsed)) result.moved_tanker = selected;
    auto const bearing{math::bearing(q8(player.position),tanker.position_q8)};
    state.angle_errors.y = angle_difference(bearing.y,player.angles.y);
    int32_t constexpr heading_limit{0x2000}, attitude_limit{0x1000};
    bool release{state.angle_errors.y >= heading_limit};
    if(!release) {
      state.angle_errors.x = magnitude(math::signed_word(player.angles.x));
      state.angle_errors.z = magnitude(math::signed_word(player.angles.z));
      release = state.angle_errors.x >= attitude_limit || state.angle_errors.z >= attitude_limit;
    }
    if(release) {
      int16_t constexpr reconnect_delay{50};
      state.delay = reconnect_delay;
      state.selected.reset();
      state.phase = 0;
      state.divisors = vec3<int16_t>{released_refuel_divisors};
    }
  } else {
    result.events.push_back({
      .kind{refuel_event_kind::tanker_model},
      .argument{0},
      .tanker{selected},
    });
    state.phase = 0;
    if(aligned(player,tanker)) {
      result.events.push_back({
        .kind{refuel_event_kind::urgent_sample},
        .argument{47},
      });
      if(follow(player,tanker,elapsed)) result.moved_tanker = selected;
    } else {
      // Native unaligned selection uses the common delay/divisor reset while
      // retaining the selected tanker and its target override.
      state.delay = 0;
      state.phase = 0;
      state.divisors = vec3<int16_t>{default_refuel_divisors};
    }
  }
  return result;
}
refuel_transfer refuelling::transfer(double const fuel, double const capacity, bool const afterburner) const {
  if(!state.phase) return {
    .fuel{fuel},
    .afterburner_warning{false},
  };
  double const updated{static_cast<double>(fuel) + assets.fuel_per_update};
  return {
    .fuel{updated > static_cast<double>(capacity) ? capacity : static_cast<double>(updated)},
    .afterburner_warning{afterburner},
  };
}

} // namespace tfx::flight
