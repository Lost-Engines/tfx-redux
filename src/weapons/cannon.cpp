#include "cannon.h"
#include <stdexcept>
#include <utility>

namespace tfx::weapons {

std::optional<release> fire_cannon(cannon_state &state, cannon_context const &context, std::span<vec3<int16_t> const> const mounts, math::random_state &random) {
  // Native keyboard path 0x86898: the F-117 has no cannon. This branch tests the flap and communications slots, not the HUD gun slot.
  if(!context.trigger || !context.gun_mode || context.aircraft == flight::player_aircraft::nighthawk) return {};
  if(!context.flaps_available) {
    random.next();
    return {};
  }
  if(!context.communications_available || context.inhibited || context.protected_player || context.crashed || context.ejected || context.landed || state.ammunition <= 0) return {};
  if(mounts.size() != 2 || state.barrel >= mounts.size()) throw std::runtime_error{"invalid alternating cannon mount state"};
  // Native 0xa2fdc consumes ten rounds per visible projectile, even when fewer than ten remain.
  state.ammunition = math::signed_word(static_cast<uint16_t>(state.ammunition - ammunition_per_release));
  vec3<int32_t> const origin{vec3<uint32_t>{context.position} * static_cast<uint32_t>(actors::position_unit)};
  auto const position{math::place_anchor(origin, context.angles, mounts[state.barrel])};
  state.barrel = static_cast<uint16_t>(1 - state.barrel);
  return release{
    .weapon{std::to_underlying(weapon_id::cannon)},
    .position{vec3<int32_t>{position}},
    .angles{vec3<math::angle>{context.angles}},
    .speed{context.speed},
    .owner{
      .pool{actor_pool::player},
    },
    .target{
      .untargeted{true},
    },
  };
}

} // namespace tfx::weapons
