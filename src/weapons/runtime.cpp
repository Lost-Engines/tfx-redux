#include "runtime.h"
#include "cannon_prediction.h"
#include "impact_prediction.h"
#include <stdexcept>
#include <utility>

namespace tfx::weapons {

runtime::runtime(std::span<definition const> const weapons, runtime_callbacks handlers, unsigned int const capacity) : definitions{weapons}, callbacks{std::move(handlers)}, slots(capacity) {
  if(!capacity || !callbacks.motion_inputs) throw std::invalid_argument{"projectile runtime needs a pool and motion inputs"};
  free.reserve(capacity);
  for(auto &slot : slots) free.push_back(&slot);
}

void runtime::prepare_untargeted_target(projectile const &shot) {
  auto const elapsed{substep.value_or(callbacks.launch_elapsed ? callbacks.launch_elapsed() : 0)};
  untargeted_point = vec3<int32_t>{shot.position};
  int32_t const distance{std::bit_cast<int32_t>(static_cast<uint32_t>(shot.speed) * 1000u * elapsed)};
  actors::move(untargeted_point,shot.angles,distance);
  untargeted_point.y = 0;
}

std::array<vec3<int32_t>,5> runtime::predict_cannon(vec3<int32_t> const &anchor_q8, vec3<math::angle> const &angles, int32_t const speed, vec2<int32_t> const &height_bounds) {
  if(definitions.empty()) throw std::out_of_range{"cannon prediction requires its weapon definition"};
  auto shot{weapons::launch(definitions[0],0,anchor_q8,angles,speed)};
  prepare_untargeted_target(shot);
  return weapons::predict_cannon(definitions[0],std::move(shot),height_bounds);
}

vec3<int32_t> runtime::predict_bomb_impact(unsigned int const weapon, vec3<int32_t> const &anchor_q8, vec3<math::angle> const &angles, int32_t const speed, vec2<int32_t> const &height_bounds) {
  if(weapon >= definitions.size()) throw std::out_of_range{"impact prediction requires a valid weapon definition"};
  if(definitions[weapon].category != bomb_impact_category) throw std::invalid_argument{"impact prediction requires a category-9 bomb"};
  auto shot{weapons::launch(definitions[weapon],static_cast<uint8_t>(weapon),anchor_q8,angles,speed)};
  prepare_untargeted_target(shot);
  return weapons::predict_bomb_impact(definitions[weapon],std::move(shot),anchor_q8,height_bounds);
}

void runtime::retire(pool_slot &slot) {
  auto const &shot{slot.value};
  if(shot.state.weapon == std::to_underlying(weapon_id::chaff) || shot.state.weapon == std::to_underlying(weapon_id::flare)) {
    for(auto *other{first}; other; other = other->next) if(other->value.target.projectile == shot.id) {
      other->value.state.motion = flight_stage::coasting;
      other->value.state.ticks = 30000;
    }
  }
  if(callbacks.target_cleared) callbacks.target_cleared(shot);
  if(callbacks.reset_visual) callbacks.reset_visual(shot.state.weapon,shot.state.secondary);
  if(slot.previous) slot.previous->next = slot.next;
  else first = slot.next;
  if(slot.next) slot.next->previous = slot.previous;
  else last = slot.previous;
  active.erase(shot.id);
  free.push_back(&slot);
}

projectile_id runtime::launch(release const &request) {
  if(request.weapon >= definitions.size()) throw std::runtime_error{"projectile weapon index exceeds the original table"};
  std::span<stage const> program{};
  if(request.jettisoned) {
    // Original DATA 0x9170 replaces the stage pointer with DATA 0x92a8,
    // the same immutable program already used by the external fuel tank.
    auto const tank{std::to_underlying(weapon_id::fuel_tank)};
    if(tank >= definitions.size()) throw std::runtime_error{"jettison requires the original dropped-store program"};
    program = definitions[tank].stages;
    if(program.empty()) throw std::runtime_error{"empty dropped-store program"};
  }
  if(!next_id) throw std::runtime_error{"projectile identity space exhausted"};
  if(free.empty()) retire(*last);
  auto &slot{*free.back()};
  free.pop_back();
  // Keep the record's address stable: a dispenser can recycle its own record during its callback.
  projectile_id const id{next_id++};
  uint8_t mount{slot.value.mount};
  if(callbacks.mount_state) {
    auto const current{callbacks.mount_state()};
    if(request.angles == current.angles) mount = current.selected;
  } else if(request.owner.pool == actor_pool::player) mount = request.mount;
  slot.value = active_projectile{
    .id{id},
    .state{weapons::launch(definitions[request.weapon],static_cast<uint8_t>(request.weapon),request.position,request.angles,request.speed,program)},
    .owner{request.owner},
    .target{request.target},
    .mount{mount},
  };
  if(slot.value.target.untargeted) prepare_untargeted_target(slot.value.state);
  slot.previous = nullptr;
  slot.next = first;
  if(first) first->previous = &slot;
  else last = &slot;
  first = &slot;
  active.emplace(id,&slot.value);
  if(callbacks.reset_visual) callbacks.reset_visual(request.weapon,false);
  if(callbacks.launched) callbacks.launched(slot.value);
  return id;
}

projectile_id runtime::countermeasure(actor_identity const owner, vec3<int32_t> const &position, bool const radar, int16_t const strength, math::random_state &random) {
  auto const decoy{launch({
    .weapon{std::to_underlying(radar ? weapon_id::chaff : weapon_id::flare)},
    .position{vec3<int32_t>{position}},
    .angles{0,0,0},
    .speed{0},
    .owner{owner},
    .target{
      .untargeted{true},
    },
  })};
  // Native 0xa35d0 visits newest first and consumes a random value even for a nonpositive signed strength.
  for(auto *slot{first}; slot; slot = slot->next) {
    auto &shot{slot->value};
    if(shot.target.actor != owner) continue;
    uint8_t const category{definitions[shot.state.weapon].category};
    if(category != static_cast<unsigned int>(radar) && category != 2) continue;
    if(static_cast<int>(random.next() & 127) >= strength) continue;
    if(callbacks.target_cleared) callbacks.target_cleared(shot);
    shot.target.actor.reset();
    shot.target.projectile = decoy;
    shot.target.position = position;
  }
  return decoy;
}

std::map<projectile_id,active_projectile const*> const &runtime::projectiles() const {
  return active;
}

active_projectile const *runtime::first_targeting_player() const {
  // Native 0x70b4c selects newest first, without a distance, motion or spent-state filter.
  for(auto const *slot{first}; slot; slot = slot->next) if(slot->value.target.actor && slot->value.target.actor->pool == actor_pool::player) return &slot->value;
  return nullptr;
}

warning_state const &runtime::player_warnings() const { return warnings; }

void runtime::finish_frame() {
  // Expired stage timers alone wait for the next motion pass. Only the spent
  // flag participates in the original post-flip removal walk.
  for(auto *slot{first}; slot;) {
    auto *const next{slot->next};
    if(slot->value.spent) retire(*slot);
    slot = next;
  }
}

void runtime::advance(simulation::timer_ticks const elapsed, math::random_state &random, bool const tfx_effects) {
  struct substep_guard {
    std::optional<simulation::timer_ticks> &value;
    ~substep_guard() { value.reset(); }
  } clear_substep{
    .value{substep},
  };
  simulation::projectile_clock clock{elapsed};
  while(auto const ticks{clock.next()}) {
    substep = *ticks;
    begin_warning_step(warnings,*ticks);
    // Native contact processing shares this stack word with the next motion
    // pass. Retail base/upper world IDs are 0/1, both non-water sound classes.
    int16_t ground_appearance{0};
    auto *slot{first};
    unsigned int budget{static_cast<unsigned int>(slots.size()) * 4096u};
    while(slot) {
      if(!budget--) throw std::runtime_error{"projectile recycling does not yield within its update budget"};
      // The original saves the next record before calling motion. It may be recycled and reinserted by a child launch.
      auto *const next{slot->next};
      auto &shot{slot->value};
      if(shot.spent || !shot.state.ticks) retire(*slot);
      else {
        auto context{callbacks.motion_inputs(shot,*ticks)};
        context.elapsed = *ticks;
        context.target = shot.target.actor ? callbacks.actor_position(*shot.target.actor) : shot.target.untargeted ? vec3<int32_t>{untargeted_point} : shot.target.designated ? callbacks.designation_position() : vec3<int32_t>{shot.target.position};
        if(shot.target.projectile) {
          auto const found{active.find(*shot.target.projectile)};
          if(found != active.end()) context.target = found->second->state.position;
        }
        auto const result{advance_motion(shot.state,definitions[shot.state.weapon],context,random,{
          .release{[&](child_release const &child) {
            launch({
              .weapon{child.weapon},
              .position{vec3<int32_t>{child.position}},
              .angles{vec3<math::angle>{child.angles}},
              .speed{child.speed},
              .owner{shot.owner},
              .target{
                .untargeted{true},
              },
            });
          }},
          .current_definition{[&](uint8_t const weapon)->definition const& { return definitions[weapon]; }},
        })};
        if(result.reset_visual && callbacks.reset_visual) callbacks.reset_visual(shot.state.weapon,shot.state.secondary);
        if(tfx_effects && definitions[shot.state.weapon].player_proximity && callbacks.player_distance) {
          int32_t const distance{callbacks.player_distance(shot)};
          auto const warning{observe_player_threat(warnings,distance,shot.target.actor && shot.target.actor->pool == actor_pool::player,context.player_ejected,random)};
          if(warning && callbacks.warning) callbacks.warning(*warning);
          if(callbacks.trail && has_smoke_trail(shot.state.motion,callbacks.camera_controls_trails ? 0 : distance)) callbacks.trail(shot);
        }
        if(tfx_effects && shot.state.position.y >= 0) {
          if(callbacks.ground_impact) callbacks.ground_impact(shot,ground_appearance);
          shot.spent = true;
        }
      }
      slot = next;
    }
    if(auto const warning{finish_warning_step(warnings)}; warning && callbacks.warning) callbacks.warning(*warning);
    // Contact traversal starts afresh, so births participate even before their ordinary first motion update.
    if(callbacks.contacts) for(auto *contact{first}; contact; contact = contact->next) callbacks.contacts(contact->value,*ticks);
  }
}

} // namespace tfx::weapons
