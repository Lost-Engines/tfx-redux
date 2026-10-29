#include "ground.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tfx::sensors {
namespace {

int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t add(int32_t const first, int32_t const second) { return std::bit_cast<int32_t>(static_cast<uint32_t>(first) + static_cast<uint32_t>(second)); }
bool available(surface_contact const &contact) { return !(contact.flags & (std::to_underlying(actors::surface_flag::destroyed) | std::to_underlying(actors::surface_flag::retired))); }

} // anonymous namespace

cruise_target acquire_cruise_target(vec3<int32_t> const &player, vec2<int32_t> const &destination_q8,
    std::function<vec2<int16_t>(vec3<int32_t> const&)> const &project) {
  int32_t constexpr target_height{-10}, projection_range_limit{32767}, offscreen{-50};
  cruise_target result{
    .position_q8{destination_q8.x,target_height * actors::position_unit,destination_q8.y},
    // The out-of-range branch writes -50 directly, even to the Q8 reticle.
    .reticle_q8{offscreen,offscreen},
    .projected{offscreen,offscreen},
  };
  auto const magnitude{[](int32_t const value) {
    return value < 0 ? std::bit_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
  }};
  int32_t const dx{add(player.x,std::bit_cast<int32_t>(0u - static_cast<uint32_t>(destination_q8.x >> actors::position_fraction_bits)))};
  int32_t const dz{add(player.z,std::bit_cast<int32_t>(0u - static_cast<uint32_t>(destination_q8.y >> actors::position_fraction_bits)))};
  if(std::max(magnitude(dx),magnitude(dz)) < projection_range_limit) {
    result.projected = project({word(destination_q8.x >> actors::position_fraction_bits),target_height,word(destination_q8.y >> actors::position_fraction_bits)});
    result.reticle_q8 = {static_cast<int32_t>(result.projected.x) * actors::position_unit,static_cast<int32_t>(result.projected.y) * actors::position_unit};
  }
  return result;
}

vec3<int32_t> ground_intersection(vec3<int32_t> const &player, math::angle const pitch, math::angle const heading) {
  // Native 0x93040 intersects the Y=0 plane, truncating distance to a signed word before the heading transform.
  int32_t const sine{math::sine_q15(pitch)}, cosine{math::cosine_q15(pitch)};
  int32_t const numerator{std::bit_cast<int32_t>((0u - static_cast<uint32_t>(player.y)) * static_cast<uint32_t>(cosine))};
  int32_t distance{0};
  if(sine && numerator) {
    if(numerator == std::numeric_limits<int32_t>::min() && sine == -1) throw std::runtime_error{"native designator division overflows"};
    distance = numerator / sine;
  }
  int16_t const radius{word(distance)};
  int32_t const yaw{-static_cast<int32_t>(heading)};
  int16_t const x{word((-static_cast<int32_t>(radius) * math::sine_q15(yaw)) >> 15)};
  int16_t const z{word((static_cast<int32_t>(radius) * math::cosine_q15(yaw)) >> 15)};
  return {add(player.x,x),0,add(player.z,z)};
}

void aim_designator(designator &state, vec3<int32_t> const &player, vec3<math::angle> const &angles, flight::player_aircraft const aircraft, designator_controls const &controls) {
  // The native gimbal is keyboard/frame based; elapsed time does not multiply these adjustments.
  auto const previous{state.pitch};
  int constexpr base_gimbal_step{256}, minimum_ground_pitch{80};
  int const step{base_gimbal_step / (state.zoom + 1)};
  state.pitch = static_cast<uint16_t>(state.pitch + (controls.up - controls.down) * step);
  state.heading = static_cast<uint16_t>(state.heading + (controls.right - controls.left) * step);
  if(math::signed_word(state.pitch) < minimum_ground_pitch) state.pitch = previous;
  if(aircraft == flight::player_aircraft::nighthawk) {
    int16_t constexpr gimbal_side_limit{0x2000}, gimbal_pitch_floor{-0x100};
    auto const yaw{word(static_cast<int32_t>(state.heading) - angles.y)};
    if(yaw < -gimbal_side_limit) state.heading = static_cast<uint16_t>(angles.y - gimbal_side_limit);
    else if(yaw > gimbal_side_limit) state.heading = static_cast<uint16_t>(angles.y + gimbal_side_limit);
    auto const pitch{word(static_cast<int32_t>(state.pitch) - angles.x)};
    if(pitch < gimbal_pitch_floor) state.pitch = static_cast<uint16_t>(angles.x + gimbal_pitch_floor);
    else if(pitch > gimbal_side_limit) state.pitch = static_cast<uint16_t>(angles.x + gimbal_side_limit);
  }
  int constexpr search_zoom_stride{450}, fallback_zoom_stride{300}, minimum_camera_clearance{30};
  state.camera = vec3<int32_t>{player};
  if(state.zoom) {
    vec3<math::angle> const direction{state.pitch,state.heading,0};
    math::advance_position(state.camera,direction,word(state.zoom * search_zoom_stride),actors::minimum_world_y,actors::maximum_world_y);
    if(state.camera.y > -minimum_camera_clearance) {
      --state.zoom;
      state.camera = vec3<int32_t>{player};
      math::advance_position(state.camera,direction,word(state.zoom * fallback_zoom_stride),actors::minimum_world_y,actors::maximum_world_y);
    }
  }
  state.point = ground_intersection(player,state.pitch,state.heading);
}

void follow_designation(designator &state, vec3<int32_t> const &player, vec3<int32_t> const &target) {
  // Native 0x92a34 follows the fixed Q8 designation; unlike search, this has no F-117 gimbal clamp.
  state.point = vec3<int32_t>{target.x >> actors::position_fraction_bits,target.y >> actors::position_fraction_bits,target.z >> actors::position_fraction_bits};
  auto const angles{math::bearing(player,state.point)};
  state.pitch = angles.x;
  state.heading = angles.y;
  state.camera = vec3<int32_t>{player};
  int constexpr zoom_stride{300}, minimum_camera_clearance{30};
  if(state.zoom) {
    vec3<math::angle> const direction{state.pitch,state.heading,0};
    math::advance_position(state.camera,direction,word(state.zoom * zoom_stride),actors::minimum_world_y,actors::maximum_world_y);
    if(state.camera.y > -minimum_camera_clearance) {
      --state.zoom;
      state.camera = vec3<int32_t>{player};
      math::advance_position(state.camera,direction,word(state.zoom * zoom_stride),actors::minimum_world_y,actors::maximum_world_y);
    }
  }
}

vec3<int32_t> mobile_target_point(vec3<int32_t> const &position, vec3<int16_t> const &offset) {
  // Native 0x7c998 deliberately ignores both the actor heading and anchor Y, keeping the aim point on Y=0.
  return {add(position.x,static_cast<int32_t>(offset.x) * actors::position_unit),0,add(position.z,static_cast<int32_t>(offset.z) * actors::position_unit)};
}

std::optional<mobile_designation> nearest_mobile_target(vec3<int32_t> const &point, std::span<surface_contact const> const contacts, int32_t const range, int16_t const type) {
  int32_t const radius{std::bit_cast<int32_t>(static_cast<uint32_t>(range) << actors::position_fraction_bits)};
  int64_t distance{static_cast<int64_t>(radius) * radius};
  std::optional<mobile_designation> result;
  for(unsigned int slot{0}; slot != actors::surface_capacity; ++slot) {
    auto const found{std::ranges::find(contacts,slot,&surface_contact::slot)};
    if(found == contacts.end() || !available(*found) || found->type != type) continue;
    for(auto const &offset : found->damage_centres) {
      auto const target{mobile_target_point(found->position,offset)};
      int32_t const x{add(target.x,std::bit_cast<int32_t>(0u - static_cast<uint32_t>(point.x)))}, z{add(target.z,std::bit_cast<int32_t>(0u - static_cast<uint32_t>(point.z)))};
      int64_t const separation{std::bit_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(x) * x) + static_cast<uint64_t>(static_cast<int64_t>(z) * z))};
      if(separation < distance) {
        distance = separation;
        result.emplace(mobile_designation{
          .slot{slot},
          .offset{vec3<int16_t>{offset}},
        });
      }
    }
  }
  return result;
}

std::optional<unsigned int> next_radiation_target(int16_t &cursor, std::span<surface_contact const> const contacts, int16_t const type, actors::allegiance const side) {
  if(cursor < -1 || cursor >= static_cast<int>(actors::surface_capacity)) throw std::runtime_error{"invalid radiation target cursor"};
  for(unsigned int tried{0}; tried != actors::surface_capacity; ++tried) {
    if(++cursor == static_cast<int>(actors::surface_capacity)) cursor = 0;
    auto const found{std::ranges::find(contacts,static_cast<unsigned int>(cursor),&surface_contact::slot)};
    if(found != contacts.end() && available(*found) && found->side == side && found->type == type) return found->slot;
  }
  return {};
}

std::optional<unsigned int> nearest_ship(vec3<int32_t> const &player, std::span<surface_contact const> const contacts, std::span<int16_t const> const allowed_types) {
  int32_t constexpr ship_acquisition_range{30000};
  int32_t distance{ship_acquisition_range};
  std::optional<unsigned int> selected;
  // The original visits slots in ascending order, independent of side; equal distances retain the first.
  for(unsigned int slot{0}; slot != actors::surface_capacity; ++slot) {
    auto const found{std::ranges::find(contacts,slot,&surface_contact::slot)};
    if(found == contacts.end() || !available(*found) || std::ranges::find(allowed_types,found->type) == allowed_types.end()) continue;
    vec3<int32_t> const position{found->position.x >> actors::position_fraction_bits,found->position.y >> actors::position_fraction_bits,found->position.z >> actors::position_fraction_bits};
    auto const separation{math::distance_estimate(player,position)};
    if(separation < distance) { selected = slot; distance = separation; }
  }
  return selected;
}

} // namespace tfx::sensors
