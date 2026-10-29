#include "landing.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>
#include "world/environment.h"

namespace tfx::flight {
namespace {

int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t subtract(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t magnitude(int32_t const value) { return value < 0 ? subtract(0,value) : value; }
int heading_degrees(math::angle const heading) {
  // Native HUD reverses the bearing, then removes at most one complete turn.
  int result{static_cast<int>((static_cast<unsigned int>(heading) + 0x8000u) * 360u >> 16)};
  if(result >= 360) result -= 360;
  return result;
}
vec3<int32_t> runway(landing_state const &state) {
  int32_t constexpr cell_centre{world::cell_units / 2};
  return {state.cell.x * world::cell_units + cell_centre + state.port.offset.x,state.carrier ? -20 : 0,state.cell.y * world::cell_units + cell_centre + state.port.offset.y};
}

} // anonymous namespace

landing_system::landing_system(landing_state initial) : state{std::move(initial)} {}
landing_state const &landing_system::value() const { return state; }
void landing_system::reset() { state = {}; }
void landing_system::disengage() { state.enabled = false; }
void landing_system::prepare_beacon() {
  auto position{runway(state)};
  position.y = 0;
  int32_t constexpr beacon_setback{0x5000};
  math::advance_position(position,{0,static_cast<math::angle>(-state.port.heading),0},beacon_setback,-16384,0);
  state.beacon_position = {position.x,position.z};
  state.beacon = true;
}
landing_selection landing_system::toggle(landing_input const &input, std::span<vec3<int32_t> const> const route,
    std::function<std::optional<landing_port>(vec2<int16_t> const&)> const &terrain,
    std::span<landing_surface const> const carriers) {
  landing_selection result;
  if(input.inhibited || input.crashed || input.ejected) return result;
  if(state.enabled) { state.enabled = false;return result; }
  result.navigation = true;
  result.afterburner_stop_sample = true;
  result.controls.emplace(landing_controls{
    .configuration{1},
    .response{input.aircraft == player_aircraft::nighthawk ? 1.0 : .5},
    .pitch_response{input.realistic ? .12 : .15},
    .roll_response{input.realistic ? .152 : .19},
    .yaw_response{input.realistic ? .024 : .03},
  });
  auto selected{select_runway(route,terrain,carriers)};
  result.waypoint = selected.waypoint;
  result.messages = std::move(selected.messages);
  if(!state.enabled) return result;
  result.music_cue = input.damaged ? 8u : 7u;
  result.controls.emplace(landing_controls{
    .configuration{3},
    .response{.8},
    .pitch_response{input.realistic ? .4 : .5},
    .roll_response{input.realistic ? .32 : .4},
    .yaw_response{input.realistic ? .048 : .06},
  });
  return result;
}
landing_selection landing_system::select_runway(std::span<vec3<int32_t> const> const route,
    std::function<std::optional<landing_port>(vec2<int16_t> const&)> const &terrain,
    std::span<landing_surface const> const carriers) {
  landing_selection result;
  state.carrier = false;
  bool found{false};
  if(!route.empty()) for(unsigned int endpoint{0}; endpoint != 2 && !found; ++endpoint) {
    unsigned int const index{endpoint ? 0u : static_cast<unsigned int>(route.size() - 1)};
    result.waypoint = index;
    auto const &point{route[index]};
    vec2<int16_t> const cell{word(point.x / world::cell_units),word(point.z / world::cell_units)};
    if(auto const port{terrain(cell)}) {
      state.cell = vec2<int16_t>{cell};state.port = *port;found = true;
    } else for(auto const &carrier : carriers) {
      // Native first arithmetic-shifts Q8 positions, then divides by a cell
      // with truncation towards zero, rather than shifting by nineteen.
      vec2<int16_t> const carrier_cell{word((carrier.position_q8.x >> 8) / world::cell_units),word((carrier.position_q8.z >> 8) / world::cell_units)};
      if(carrier_cell != cell) continue;
      if(carrier.destroyed) result.messages.push_back(landing_radio::carrier_destroyed);
      else { state.cell = vec2<int16_t>{cell};state.port = carrier.port;state.carrier = true;found = true;break; }
    }
  }
  state.enabled = found;
  if(!found) { result.messages.push_back(landing_radio::no_runway);return result; }
  prepare_beacon();
  result.messages.push_back(landing_radio::engaged);
  return result;
}
landing_cue landing_system::advance(vec3<int32_t> const &player, bool const speed_request,
    std::function<landing_projection(vec3<int32_t> const&)> const &project) {
  landing_cue cue{
    .beacon{state.beacon},
  };
  if(state.beacon) {
    cue.speed_request = speed_request;
    vec3<int32_t> const origin{state.beacon_position.x,0,state.beacon_position.y};
    cue.heading = heading_degrees(math::bearing(origin,{player.x,0,player.z}).y);
    cue.projected = project({origin.x,player.y,origin.z});
    cue.projected.valid = cue.projected.valid && static_cast<uint16_t>(cue.projected.point.x) < 319 && static_cast<uint16_t>(cue.projected.point.y) < 199;
    int32_t constexpr beacon_reached_distance{1000};
    if(std::max(magnitude(subtract(origin.x,player.x)),magnitude(subtract(origin.z,player.z))) < beacon_reached_distance) state.beacon = false;
    return cue;
  }
  if(!state.carrier) {

    float constexpr approach_run{700}, normalised_sine{1.0f / 32767.0f};
    // Both float constants are multiplied in the original extended-precision
    // register before truncation; the word offsets overwrite authored values.
    state.port.offset = {
      word(static_cast<int32_t>(static_cast<long double>(math::sine_q15(state.port.heading)) * approach_run * normalised_sine)),
      word(static_cast<int32_t>(static_cast<long double>(math::cosine_q15(state.port.heading)) * approach_run * normalised_sine)),
    };
  }
  auto const destination{runway(state)};
  state.bearing = math::bearing(destination,player);
  cue.heading = heading_degrees(state.bearing.y);
  state.bearing.y = static_cast<math::angle>(-state.bearing.y);
  cue.projected = project({destination.x,0,destination.z});
  cue.projected.valid = cue.projected.valid && static_cast<uint16_t>(cue.projected.point.x) < 320 && static_cast<uint16_t>(cue.projected.point.y) < 199;
  int constexpr heading_limit{0x1000}, pitch_limit{0x800}, glide_angle{-1274};
  state.correction.x = static_cast<int16_t>(-std::clamp<int>(word(state.port.heading - state.bearing.y),-heading_limit,heading_limit) / 170);
  state.pitch_error = std::clamp(glide_angle - math::signed_word(state.bearing.x),-pitch_limit,pitch_limit);
  state.correction.y = static_cast<int16_t>(state.pitch_error / 85);
  cue.correction = vec2<int16_t>{state.correction};
  int constexpr approach_tolerance{2};
  if(state.correction.y < -approach_tolerance) cue.notice = state.height = approach_height::low;
  else if(state.correction.y > approach_tolerance) cue.notice = state.height = approach_height::high;
  else if(!state.correction.x && !state.correction.y) cue.notice = state.height = approach_height::aligned;
  return cue;
}

} // namespace tfx::flight
