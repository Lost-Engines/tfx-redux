#include "surface_combat.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "world/environment.h"

namespace tfx::actors {
namespace {

int32_t subtract(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? subtract(0,value) : value;
}
int32_t horizontal_range(vec3<int32_t> const &from, vec3<int32_t> const &to) {
  return std::max(magnitude(subtract(to.x,from.x)),magnitude(subtract(to.z,from.z)));
}
weapons::release make_shot(surface_state const &state, unsigned int const owner, uint16_t const weapon, vec3<int32_t> const &target, math::angle const pitch, std::optional<unsigned int> const actor = {}) {
  auto const bearing{math::bearing(state.pose.position,target)};
  int32_t constexpr minimum_launch_speed{200};
  weapons::target_reference destination{
    .position{vec3<int32_t>{target}},
  };
  if(actor) destination.actor = weapons::actor_identity{
    .pool{*actor == player_air_target ? weapons::actor_pool::player : weapons::actor_pool::aircraft},
    .index{*actor == player_air_target ? 0u : *actor},
  };
  return {
    .weapon{weapon},
    .position{vec3<int32_t>{state.pose.position}},
    .angles{pitch,bearing.y,0},
    .speed{std::max(minimum_launch_speed,state.pose.speed >> speed_fraction_bits)},
    .owner{
      .pool{weapons::actor_pool::surface},
      .index{owner},
    },
    .target{destination},
  };
}

} // anonymous namespace

std::vector<weapons::release> engage_surface_targets(surface_state &state, profile const &profile, surface_combat_context const &context, math::random_state &random) {
  // Native 0x7be44: player first for hostile/neutral units, followed by the opposing aircraft pool backwards.
  resources::binary_view const native{profile.source};
  int32_t constexpr q8_units_per_mile{5280 * 32};
  int32_t const range{std::bit_cast<int32_t>(static_cast<uint32_t>(native.le16(42)) * q8_units_per_mile)};
  std::vector<weapons::release> shots;
  if(!range || std::to_underlying(profile.side) > std::to_underlying(allegiance::neutral)) return shots;
  auto const fire{[&](vec3<int32_t> const &target, unsigned int const actor){
    unsigned int constexpr firing_chance_divisor{60};
    if(random.next() % firing_chance_divisor) return false;
    math::angle constexpr vertical_launch_pitch{0xc000};
    shots.push_back(make_shot(state,context.owner,native.byte(44),target,vertical_launch_pitch,actor));
    return true;
  }};
  if(profile.side != allegiance::allied && state.player_shots < native.byte(45)) {
    int32_t separation{horizontal_range(state.pose.position,context.player)};
    int32_t detection_range{range};
    if(context.aircraft == flight::player_aircraft::nighthawk) {
      separation = std::max(separation,magnitude(subtract(context.player.y,state.pose.position.y)));
      detection_range /= 8;
    }
    if(separation <= detection_range && fire(context.player,player_air_target)) ++state.player_shots;
  }
  unsigned int const base{profile.side == allegiance::allied ? 0u : aircraft_per_side};
  for(unsigned int reverse{aircraft_per_side}; reverse; --reverse) {
    unsigned int const actor{base + reverse - 1};
    auto const found{std::ranges::find(context.aircraft_targets,actor,&surface_air_target::actor)};
    // The native enumerator omits retired slots, but does not omit a falling aircraft merely because it has crashed.
    if(found == context.aircraft_targets.end() || (found->flags & std::to_underlying(air_flag::retired))) continue;
    if(horizontal_range(state.pose.position,found->position) < range) fire(found->position,actor);
  }
  return shots;
}

std::optional<weapons::release> surface_ground_attack(surface_state const &state, unsigned int const owner, surface_request const &request, bool const intact) {
  // Native 0x7bee8 fires once per authored attack command, only if that terrain assembly is present and alive.
  if(request.kind != surface_request_kind::ground_target) throw std::invalid_argument{"surface ground attack requires an attack command"};
  if(!intact) return {};
  vec3<int32_t> const target{
    std::bit_cast<int32_t>(static_cast<uint32_t>(request.cell.x) * world::cell_units * position_unit + world::cell_units / 2 * position_unit),
    0,
    std::bit_cast<int32_t>(static_cast<uint32_t>(request.cell.y) * world::cell_units * position_unit + world::cell_units / 2 * position_unit),
  };
  math::angle constexpr ground_launch_pitch{0x1ffe};
  return make_shot(state,owner,request.weapon,target,ground_launch_pitch);
}

} // namespace tfx::actors
