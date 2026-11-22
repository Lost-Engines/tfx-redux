#include "projectile.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace tfx::weapons {
namespace {

unsigned int constexpr bomblet_weapon{23}, harpoon_weapon{22};
int constexpr guidance_turn_per_tick{140}, guidance_capture_angle{10880};
int constexpr terminal_fall_speed{25000}, gravity_velocity_divisor{50}, gravity_displacement_divisor{250};

int32_t wrap(int64_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value));
}
int16_t word(int32_t const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
void expire(projectile &state, definition const &weapon, uint16_t const elapsed) {
  state.ticks = word(state.ticks - elapsed);
  if(state.ticks <= 0) next_stage(state, weapon);
}
void travel(projectile &state, motion_context const &context) {
  actors::move(state.position, state.angles, wrap(static_cast<int64_t>(context.elapsed) * state.speed), context.height_bounds.x, context.height_bounds.y);
}
void gravity(projectile &state, uint16_t const elapsed, int const acceleration = 20) {
  int32_t const displacement{wrap(static_cast<int64_t>(elapsed) * state.fall_speed) / gravity_velocity_divisor + (static_cast<int32_t>(elapsed) * elapsed) / 250};
  state.position.y = wrap(static_cast<int64_t>(state.position.y) + displacement);
  state.fall_speed = std::min<int16_t>(word(state.fall_speed + elapsed * acceleration), terminal_fall_speed);
}
void consume_range(projectile &state, definition const &weapon, uint16_t const elapsed) {
  state.range = wrap(static_cast<int64_t>(state.range) - (wrap(static_cast<int64_t>(elapsed) * state.speed) >> 8));
  if(state.range < 0) next_stage(state, weapon);
}

} // anonymous namespace

bool has_swept_contacts(flight_stage const motion) {
  return motion != flight_stage::attached && motion != flight_stage::falling && motion != flight_stage::accelerating;
}

bool has_smoke_trail(flight_stage const motion, int32_t const player_distance) {
  int32_t constexpr maximum_emission_distance{4000};
  // Native 0xa3378 excludes attached/falling callbacks, independently of the
  // three-callback contact gate; accelerating stores can already leave smoke.
  return motion != flight_stage::attached && motion != flight_stage::falling && player_distance < maximum_emission_distance;
}

void next_stage(projectile &state, definition const &weapon) {
  auto const program{state.program.empty() ? std::span<stage const>{weapon.stages} : state.program};
  if(state.next_stage >= program.size()) throw std::runtime_error{"projectile leaves its authored stage program"};
  auto const &stage{program[state.next_stage++]};
  state.motion = stage.motion;
  state.ticks = word(stage.ticks);
}

projectile launch(definition const &weapon, uint8_t const index, vec3<int32_t> const &position, vec3<math::angle> const &angles, int32_t const speed, std::span<stage const> const program) {
  projectile result{
    .position{vec3<int32_t>{position}},
    .previous{vec3<int32_t>{position}},
    .angles{angles.x, angles.y, 0},
    .speed{wrap(static_cast<int64_t>(speed) + weapon.launch_speed)},
    .range{weapon.range},
    .weapon{index},
    .program{program},
  };
  next_stage(result, weapon);
  return result;
}

motion_result advance_motion(projectile &state, definition const &weapon, motion_context const &context, math::random_state &random, motion_hooks const &hooks) {
  motion_result result;
  if(state.motion == flight_stage::finished) return result;
  state.previous = state.position;
  uint16_t const elapsed{context.elapsed};
  auto const current_weapon{[&]()->definition const & {return hooks.current_definition ? hooks.current_definition(state.weapon) : weapon;}};
  auto const falling{[&]{
    expire(state, current_weapon(), elapsed);
    travel(state, context);
    gravity(state, elapsed);
  }};
  auto const scatter{[&](vec3<int32_t> const &centre, bool const wide){
    uint16_t const z{static_cast<uint16_t>(random.next() & (wide ? 65535u : 511u))};
    uint16_t const y{static_cast<uint16_t>(random.next() & (wide ? 511u : 2047u))};
    uint16_t const x{static_cast<uint16_t>(random.next() & (wide ? 65535u : 511u))};
    child_release child{
      .position{wrap(static_cast<int64_t>(centre.x) + x), wrap(static_cast<int64_t>(centre.y) + y), wrap(static_cast<int64_t>(centre.z) + z)},
      .angles{vec3<math::angle>{context.player_angles}},
      .speed{context.player_speed},
      .weapon{bomblet_weapon},
    };
    if(hooks.release) {
      hooks.release(child);
    } else {
      result.children.push_back(std::move(child));
    }
  }};
  switch(state.motion) {
  case flight_stage::attached:
    expire(state, current_weapon(), elapsed);
    if(context.player_owned) {
      state.position = context.launch_anchor;
    } else {
      state.motion = flight_stage::falling;
      falling();
    }
    break;
  case flight_stage::falling:
    falling();
    break;
  case flight_stage::accelerating:
  case flight_stage::accelerating_alias:
    state.secondary = true;
    result.reset_visual = true;
    expire(state, current_weapon(), elapsed);
    state.speed = wrap(static_cast<int64_t>(state.speed) + (state.acceleration >> 2));
    state.acceleration = std::min<int16_t>(word(state.acceleration + elapsed * 16), 200);
    travel(state, context);
    break;
  case flight_stage::coasting:
    state.secondary = true;
    result.reset_visual = true;
    falling();
    break;
  case flight_stage::falling_store:
    expire(state, current_weapon(), elapsed);
    gravity(state, elapsed, 50);
    if(state.weapon != bomblet_weapon) state.angles.x = static_cast<math::angle>(std::min<int16_t>(word(state.angles.x + elapsed * 400), 16384));
    break;
  case flight_stage::retarded_store:
    gravity(state, elapsed, 50);
    if(state.weapon != bomblet_weapon) state.angles.x = static_cast<math::angle>(std::min<int16_t>(word(state.angles.x + elapsed * 80), 16384));
    if(state.position.y > -12800) {
      state.speed = wrap(static_cast<int64_t>(state.speed) + (state.acceleration >> 8));
      state.acceleration = std::min<int16_t>(word(state.acceleration + elapsed * 16), 300);
    }
    break;
  case flight_stage::attached_dispenser:
    for(unsigned int count{0}; count != (elapsed >> 4) + 1u; ++count) scatter(context.launch_anchor, false);
    falling();
    if(state.position.y > -76800) {
      state.speed = wrap(static_cast<int64_t>(state.speed) + (state.acceleration >> 8));
      state.acceleration = std::min<int16_t>(word(state.acceleration + elapsed), 200);
    }
    break;
  case flight_stage::falling_dispenser:
    for(unsigned int count{0}; count != elapsed + 1u; ++count) scatter(state.position, true);
    falling();
    break;
  case flight_stage::guided:
  case flight_stage::sea_skimming:
    {
    bool const sea_skimming{state.motion == flight_stage::sea_skimming};
    if(state.weapon == harpoon_weapon && state.position.y <= -20480) state.position.y = wrap(static_cast<int64_t>(state.position.y) + elapsed * 512);
    if(context.target_is_player && context.player_ejected) state.motion = flight_stage::decoyed;
    if(context.target_is_player && context.player_decoys && weapon.category == 1 && (random.next() & 127) < 60 && context.player_distance > 1000) state.motion = flight_stage::decoyed;
    auto const desired{math::bearing(state.position, context.target)};
    auto const error{[](math::angle const a, math::angle const b){
      int const difference{math::signed_word(a) - math::signed_word(b)};
      // Original 0xa3b6c negates a wide difference across the seam instead of reducing it modulo one turn.
      return std::abs(difference) > 32767 ? -difference : difference;
    }};
    int pitch{error(desired.x, state.angles.x)};
    int heading{error(desired.y, state.angles.y)};
    int const limit{elapsed * guidance_turn_per_tick};
    bool const track{(std::abs(pitch) < guidance_capture_angle && std::abs(heading) < guidance_capture_angle) || weapon.category == 1 || weapon.category == 13 || weapon.category == 3};
    if(track) {
      int const bank{math::signed_word(state.angles.z)};
      state.angles.z = static_cast<math::angle>(bank + ((-(heading >> 2) - bank) >> 4));
      pitch = std::clamp(pitch, -limit, limit);
      heading = std::clamp(heading, -limit, limit);
      state.angles.x = static_cast<math::angle>(state.angles.x + pitch);
      state.angles.y = static_cast<math::angle>(state.angles.y + heading);
    } else if(math::signed_word(state.angles.x) < guidance_capture_angle) state.angles.x = static_cast<math::angle>(state.angles.x + word(elapsed * guidance_turn_per_tick));
    travel(state, context);
    consume_range(state, current_weapon(), elapsed);
    if(sea_skimming && state.position.y >= -20480) next_stage(state, current_weapon());
  }
    break;
  case flight_stage::decoyed:
    if(!context.player_decoys) state.motion = flight_stage::guided;
    if(state.ticks & 32) {
      state.angles.x = static_cast<math::angle>(state.angles.x - word(elapsed * guidance_turn_per_tick));
      state.angles.y = static_cast<math::angle>(state.angles.y + word(elapsed * guidance_turn_per_tick));
    } else {
      state.angles.x = static_cast<math::angle>(state.angles.x + word(elapsed * guidance_turn_per_tick));
      state.angles.y = static_cast<math::angle>(state.angles.y - word(elapsed * guidance_turn_per_tick));
    }
    travel(state, context);
    consume_range(state, current_weapon(), elapsed);
    break;
  case flight_stage::finished:
    break;
  }
  return result;
}

} // namespace tfx::weapons
