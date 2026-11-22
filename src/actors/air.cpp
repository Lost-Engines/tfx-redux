#include "air.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>
#include <utility>
#include "math/attitude.h"
#include "world/environment.h"

namespace tfx::actors {
namespace {

unsigned int constexpr controller_budget{1024}, burst_shift{10}, burst_mask{63u << burst_shift};
int constexpr fixed_units_per_foot{32}, fixed_units_per_mile{5280 * fixed_units_per_foot};
int constexpr manoeuvre_pitch_tolerance{0xda4}, manoeuvre_capture_angle{64};
int constexpr nose_capture_angle{0x1554}, tail_capture_angle{0x5550};
int constexpr waypoint_near_radius{world::cell_units * position_unit};

int32_t wrap(int64_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value));
}
int16_t word(int const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
int32_t absolute(int32_t const value) {
  return value < 0 ? wrap(-static_cast<int64_t>(value)) : value;
}
bool flagged(air_actor const &actor, air_flag const flag) {
  return (actor.flight.pose.flags & std::to_underlying(flag)) != 0;
}
void set_flag(air_actor &actor, air_flag const flag, bool const enabled = true) {
  if(enabled) actor.flight.pose.flags |= std::to_underlying(flag);
  else actor.flight.pose.flags &= static_cast<uint16_t>(~std::to_underlying(flag));
}
void push(air_actor &actor, air_action const action) {
  if(!(actor.actions >> 24)) actor.actions = (actor.actions << 8) | std::to_underlying(action);
}
int32_t difference(int32_t const a, int32_t const b) {
  return wrap(static_cast<int64_t>(a) - b);
}
int32_t horizontal_distance(vec3<int32_t> const &a, vec3<int32_t> const &b) {
  return std::max(absolute(difference(a.x,b.x)),absolute(difference(a.z,b.z)));
}
vec3<int32_t> cell_position(int16_t const x, int16_t const z, int32_t const y) {
  return {
    wrap(static_cast<int64_t>(x) * world::cell_units * position_unit + world::cell_units / 2 * position_unit),
    y,
    wrap(static_cast<int64_t>(z) * world::cell_units * position_unit + world::cell_units / 2 * position_unit),
  };
}
void canonicalise(initial_actor &pose) {
  // Native 0x75d78 canonicalises an inverted Euler representation after manoeuvres and fatal damage.
  int16_t const pitch{word(pose.angles.x)};
  if(pitch == -32768 || std::abs(static_cast<int>(pitch)) > 16384) {
    pose.angles.x = static_cast<math::angle>(32768 - pose.angles.x);
    pose.angles.y = static_cast<math::angle>(pose.angles.y - 32768);
    pose.angles.z = static_cast<math::angle>(pose.angles.z - 32768);
  }
}
void approach_heading(air_actor &actor, int16_t const error, air_control_context const &context) {
  if(command_turn(actor.flight,actor.performance,error,context)) command_speed(actor.flight,actor.performance,static_cast<int32_t>(cruise_speed(actor.flight,actor.performance,context.turn_envelope)) << speed_fraction_bits);
}

} // anonymous namespace

class air_runtime::request_queue {
private:
  std::vector<air_request> &output;
  air_context const &context;

public:
  request_queue(std::vector<air_request> &output, air_context const &context) : output{output}, context{context} {}
  template<typename data_type> void emplace(unsigned int const actor, data_type data) {
    auto &request{output.emplace_back()};
    request.actor = actor;
    request.data.template emplace<data_type>(std::move(data));
    if(context.emitted) context.emitted(request);
  }
  void push_back(air_request request) {
    output.push_back(std::move(request));
    if(context.emitted) context.emitted(output.back());
  }
};

void air_runtime::apply_refuelling_pose(unsigned int const index, vec3<int32_t> const &position_q8, vec3<math::angle> const &angles) {
  if(index < aircraft_per_side || !actors.at(index)) throw std::invalid_argument{"refuelling pose requires an allied aircraft"};
  auto &pose{actors[index]->flight.pose};
  pose.position = vec3<int32_t>{position_q8};
  pose.angles = vec3<math::angle>{angles};
}

unsigned int air_registry_index(initial_actor const &actor) {
  if(actor.kind != record_kind::aircraft || actor.slot >= aircraft_per_side) throw std::invalid_argument{"invalid air pool identity"};
  return (actor.side == allegiance::allied ? aircraft_per_side : 0) + actor.slot;
}

air_runtime::air_runtime(program const &program, std::span<initial_actor const> const initial, std::span<profile const> const profiles, std::span<formation_offset const> const offsets, std::span<uint8_t const> const envelope, math::random_state const seed, std::span<manoeuvre const> const sequences) : formation{offsets}, envelope{envelope}, random{seed}, manoeuvres{sequences} {
  if(formation.size() != aircraft_per_group) throw std::invalid_argument{"missing air formation table"};
  for(unsigned int index{0}; index != manoeuvres.size(); ++index) manoeuvre_banks.at(manoeuvres[index].category).push_back(index);
  for(auto const &record : program.records) commands.push_back(record.body);
  for(auto const &actor : initial) {
    if(actor.kind != record_kind::aircraft) continue;
    auto const profile{std::ranges::find(profiles,actor.profile_id,&actors::profile::id)};
    if(profile == profiles.end()) throw std::runtime_error{"missing air profile"};
    resources::binary_view const native{profile->native_window.empty() ? profile->source : profile->native_window};
    actors.at(air_registry_index(actor)) = air_actor{
      .flight{
        .pose{actor},
      },
      .performance{decode_air_performance(*profile)},
      .location{
        .record{actor.record},
        .address{0},
      },
      .combat{
        .maximum_player_shots{native.le16(60)},
        .firing_delay{native.le16(64)},
        .firing_cooldown{native.le16(66)},
        .gun_range{native.le16(68)},
        .evasion_range{native.le16(70)},
        .engagement_range{native.le16(72)},
      },
    };
  }
}

math::random_state &air_runtime::random_stream() {
  return random;
}

std::vector<air_request> air_runtime::fatal_damage(unsigned int const index, air_loss_cause const cause) {
  auto &actor{actors.at(index).value()};
  auto &state{actor.flight};
  canonicalise(state.pose);
  if(flagged(actor,air_flag::crashed)) return {};
  actor.wait_seconds = 5;
  state.input_ticks = 0;
  set_flag(actor,air_flag::crashed);
  // Native 0x785ec forces a collision fall; 0x78274 additionally samples combat ejection.
  bool const eject{cause == air_loss_cause::combat && (random.next() & 7) < 3};
  if(eject) {
    actor.actions = std::to_underlying(air_action::retire_ejected);
    state.input_ticks = air_control_horizon;
    state.acceleration = 0;
    state.angular_rate.x = 0;
    state.angular_rate.y = 0;
  } else actor.actions = std::to_underlying(air_action::falling);
  return {{
    .actor{index},
    .data{air_scenery_event{
      .entry{eject ? 18u : 17u},
    }},
  }};
}

std::span<std::optional<air_actor> const> air_runtime::aircraft() const {
  return actors;
}

std::span<uint8_t const> air_runtime::program_bytes(unsigned int const record) const {
  return commands.at(record);
}

vec3<int32_t> air_runtime::formation_position(vec3<int32_t> const &position, math::angle const heading, unsigned int const member) const {
  auto result{vec3<int32_t>{position}};
  auto const &offset{formation[member]};
  move(result,{0,heading,0},offset.forward_distance);
  move(result,{0,static_cast<math::angle>(heading + offset.heading),0},offset.distance);
  result.y = wrap(static_cast<int64_t>(result.y) + offset.height);
  return result;
}

void air_runtime::course(air_actor &actor, vec3<int32_t> const &destination, int32_t const distance, bool const pitch, air_control_context const &context) {
  shorten_air_step(actor.flight,distance);
  auto const angles{math::bearing(actor.flight.pose.position,destination)};
  if(pitch) command_pitch(actor.flight,actor.performance,static_cast<math::angle>(altitude_pitch(actor.flight,actor.performance,destination.y)),context);
  approach_heading(actor,word(angles.y - actor.flight.pose.angles.y),context);
}

bool air_runtime::follow_formation(air_actor &actor, air_control_context const &context) {
  auto &pose{actor.flight.pose};
  if(pose.leader == 255 || flagged(actor,air_flag::waypoint_steering) || !flagged(actor,air_flag::airborne)) return false;
  auto const pool{pose.side == allegiance::allied ? aircraft_per_side : 0};
  auto const &leader{actors.at(pool + pose.leader)};
  if(!leader || flagged(*leader,air_flag::crashed) || flagged(*leader,air_flag::retired)) { pose.leader = 255; return false; }
  auto const destination{formation_position(leader->flight.pose.position,leader->flight.pose.angles.y,pose.formation)};
  int32_t const distance{std::max(horizontal_distance(pose.position,destination),absolute(difference(destination.y,pose.position.y)))};
  auto const &offset{formation[pose.formation]};
  auto const capture{wrap(static_cast<int64_t>(offset.forward_distance) + offset.distance)};
  if(flagged(actor,air_flag::joining_formation)) {
    if(distance <= capture) {
      set_flag(actor,air_flag::joining_formation,false);
      if(actor.location != leader->location) { actor.location = leader->location; pose.flags |= burst_mask; }
      return false;
    }
  } else {
    if(distance <= wrap(static_cast<int64_t>(capture) * 8)) return false;
    set_flag(actor,air_flag::joining_formation);
  }
  auto const angles{math::bearing(pose.position,destination)};
  shorten_air_step(actor.flight,distance);
  command_pitch(actor.flight,actor.performance,angles.x,context);
  int32_t speed{wrap(static_cast<int64_t>(leader->flight.pose.speed) + (static_cast<int64_t>(distance / (100 * fixed_units_per_foot)) << speed_fraction_bits))};
  if(command_turn(actor.flight,actor.performance,word(angles.y - pose.angles.y),context)) speed = static_cast<int32_t>(cruise_speed(actor.flight,actor.performance,envelope)) << speed_fraction_bits;
  command_speed(actor.flight,actor.performance,speed);
  return true;
}

air_contact air_runtime::contact(unsigned int const target, air_context const &context) const {
  if(target == player_air_target) {
    if(!context.player) throw std::runtime_error{"air combat target has no player state"};
    return air_contact{*context.player};
  }
  auto const &pose{actors.at(target).value().flight.pose};
  return {
    .position{vec3<int32_t>{pose.position}},
    .angles{vec3<math::angle>{pose.angles}},
    .speed{pose.speed},
  };
}

bool air_runtime::acquire(unsigned int const index, std::optional<unsigned int> const excluded, air_context const &context) {
  auto &actor{*actors[index]};
  auto const &pose{actor.flight.pose};
  if(!flagged(actor,air_flag::airborne)) return false;
  if(index < context.threats.size() && context.threats[index] && horizontal_distance(pose.position,context.threats[index]->position) <= wrap(static_cast<int64_t>(actor.combat.evasion_range) * fixed_units_per_mile)) {
    push(actor,air_action::evasive);
    actor.target.reset();
    return true;
  }
  int32_t const range{wrap(static_cast<int64_t>(actor.combat.engagement_range) * fixed_units_per_mile)};
  if(!range) return false;
  if(pose.side != allegiance::allied && flagged(actor,air_flag::hostile) && context.player && excluded != player_air_target) {
    int32_t distance{horizontal_distance(pose.position,context.player->position)};
    bool const stealth{context.player_aircraft == flight::player_aircraft::nighthawk};
    if(stealth) distance = std::max(distance,absolute(difference(pose.position.y,context.player->position.y)));
    if(distance <= (stealth ? range / 8 : range)) {
      actor.target = player_air_target;
      push(actor,air_action::engagement);
      return true;
    }
  }
  unsigned int const base{pose.side == allegiance::allied ? 0 : aircraft_per_side};
  unsigned int cursor{actor.scan_cursor % aircraft_per_side};
  // Native 0x76a78 amortises acquisition over six candidates per controller visit.
  for(unsigned int scanned{0}; scanned != aircraft_per_side / 8; ++scanned) {
    unsigned int const candidate{base + cursor};
    auto const &other{actors[candidate]};
    bool const threatened{candidate < context.threats.size() && context.threats[candidate].has_value()};
    if(other && excluded != candidate && !flagged(*other,air_flag::retired) && !flagged(*other,air_flag::crashed) && !threatened && horizontal_distance(pose.position,other->flight.pose.position) < range) {
      actor.target = candidate;
      actor.scan_cursor = static_cast<uint8_t>(cursor);
      push(actor,air_action::engagement);
      return true;
    }
    cursor = (cursor + 1) % aircraft_per_side;
  }
  actor.scan_cursor = static_cast<uint8_t>(cursor);
  return false;
}

void air_runtime::select_manoeuvre(air_actor &actor, unsigned int const category) {
  actor.manoeuvre.reset();
  auto const &bank{manoeuvre_banks.at(category)};
  if(bank.empty()) return;
  unsigned int const program{bank[random.next() % bank.size()]};
  auto const &sequence{manoeuvres[program]};
  auto const &pose{actor.flight.pose};
  if(pose.speed < static_cast<int32_t>(sequence.minimum_speed) * (1 << speed_fraction_bits)) return;
  if(wrap(static_cast<int64_t>(pose.position.y) + sequence.relative_altitude) >= maximum_world_y * position_unit) return;
  int16_t const error{word(sequence.entry_angles.x - pose.angles.x)};
  if(error == -32768 || std::abs(static_cast<int>(error)) > manoeuvre_pitch_tolerance) return;
  if(random.next() & (category == 0 ? 7 : 1)) return;
  actor.manoeuvre = manoeuvre_cursor{
    .program{program},
  };
}

void air_runtime::countermeasures(unsigned int const index, air_context const &context, request_queue &requests) {
  if(index >= context.threats.size() || !context.threats[index]) return;
  auto const &threat{*context.threats[index]};
  auto const &actor{*actors[index]};
  unsigned int const mask{horizontal_distance(actor.flight.pose.position,threat.position) > 3 * fixed_units_per_mile ? 15u : 3u};
  if(random.next() & mask) return;
  // The original switches on the projectile weapon index itself, not its guidance category.
  bool const infrared{threat.weapon == 0 || (threat.weapon != 1 && (random.next() & 1))};
  uint32_t const dividend{actor.performance.turn_dividend};
  int32_t const square{std::bit_cast<int32_t>(dividend * dividend)};
  requests.push_back({
    .actor{index},
    .data{air_countermeasure{
      .kind{infrared ? air_countermeasure_kind::infrared : air_countermeasure_kind::radar},
      .strength{word(square / 4)},
    }},
  });
}

bool air_runtime::combat_action(unsigned int const index, unsigned int const action, air_context const &world, request_queue &requests) {
  auto &actor{*actors[index]};
  auto &state{actor.flight};
  auto &pose{state.pose};
  air_control_context const context{
    .model{world.model},
    .turn_envelope{envelope},
  };
  auto const replace{[&](air_action const next){ actor.actions = (actor.actions & ~255u) | std::to_underlying(next); }};
  auto const cruise_turn{[&](int16_t const heading){
    command_turn(state,actor.performance,heading,context);
    command_speed(state,actor.performance,static_cast<int32_t>(cruise_speed(state,actor.performance,envelope)) << speed_fraction_bits);
  }};
  auto const choose{[&](unsigned int const category, bool const replace_action = false){
    select_manoeuvre(actor,category);
    if(!actor.manoeuvre) return false;
    if(replace_action) replace(air_action::align_manoeuvre);
    else push(actor,air_action::align_manoeuvre);
    return true;
  }};
  switch(static_cast<air_action>(action)) {
  case air_action::falling:
    if(pose.position.y >= maximum_world_y * position_unit) {
      vec3<int32_t> const position{pose.position.x >> position_fraction_bits,pose.position.y >> position_fraction_bits,pose.position.z >> position_fraction_bits};
      requests.push_back({
        .actor{index},
        .data{air_impact{
          .position{vec3<int32_t>{position}},
          .kind{world.impact_effect ? world.impact_effect(position) : 8},
        }},
      });
      pose.speed = 0;
      state.input_ticks = 120;
      actor.actions = std::to_underlying(air_action::retire_crashed);
      set_flag(actor,air_flag::crashed);
      return true;
    }
    reset_air_controls(state);
    if(word(pose.angles.x) < 0x2aa8) pose.angles.x = static_cast<math::angle>(pose.angles.x + 127);
    state.angular_rate.y = 127;
    state.angular_rate.z = actor.performance.maximum_roll_rate;
    if(pose.speed < static_cast<int32_t>(actor.performance.maximum_speed) << speed_fraction_bits) command_speed(state,actor.performance,static_cast<int32_t>(actor.performance.maximum_speed) << speed_fraction_bits);
    state.input_ticks = 1;
    return true;
  case air_action::align_manoeuvre:
    {
      auto &cursor{actor.manoeuvre.value()};
      auto const &sequence{manoeuvres[cursor.program]};
      state.input_ticks = air_control_horizon;
      bool aligned{true};
      for(unsigned int axis : {0u,2u}) {
        auto const target{axis == 0 ? sequence.entry_angles.x : sequence.entry_angles.y};
        int16_t const error{word(target - pose.angles[axis])};
        if(error != -32768 && std::abs(static_cast<int>(error)) < manoeuvre_capture_angle) {
          pose.angles[axis] = target;
          state.angular_rate[axis] = 0;
        } else {
          state.angular_rate[axis] = static_cast<int16_t>(error / air_control_horizon);
          aligned = false;
        }
      }
      if(!aligned) return true;
      cursor.address = manoeuvre_header_bytes;
      replace(air_action::manoeuvre);
      return false;
    }
  case air_action::manoeuvre:
    {
      auto &cursor{actor.manoeuvre.value()};
      resources::binary_view const code{manoeuvres[cursor.program].source};
      unsigned int const pc{cursor.address};
      state.input_ticks = static_cast<int16_t>(code.byte(pc));
      if(!state.input_ticks) {
        actor.manoeuvre.reset();
        canonicalise(pose);
        replace(air_action::normal);
        return false;
      }
      state.angular_rate = vec3<int16_t>{static_cast<int8_t>(code.byte(pc + 1)),static_cast<int8_t>(code.byte(pc + 2)),static_cast<int16_t>(static_cast<int8_t>(code.byte(pc + 3)) * 4)};
      state.acceleration = static_cast<int8_t>(code.byte(pc + 4)) * 256;
      cursor.address += manoeuvre_step_bytes;
      if(unsigned int const entry{code.byte(pc + 5)}) requests.push_back({
        .actor{index},
        .data{air_scenery_event{
          .entry{entry},
        }},
      });
      countermeasures(index,world,requests);
      int16_t const maximum{word(maximum_turn_rate(state,actor.performance,context))};
      if(state.angular_rate.y > maximum) state.angular_rate.y = maximum;
      return true;
    }
  case air_action::evasive:
    {
      if(index >= world.threats.size() || !world.threats[index]) { actor.actions >>= 8; return false; }
      countermeasures(index,world,requests);
      reset_air_controls(state);
      int16_t heading{word(math::bearing(pose.position,world.threats[index]->position).y - pose.angles.y)};
      int const magnitude{std::abs(static_cast<int>(heading))};
      if(magnitude >= 0x1c70 && magnitude <= 0x6388 && choose(1,true)) return false;
      heading = word(heading + (heading > 0 ? 0xc004 : 0x3ffc));
      cruise_turn(heading);
      return true;
    }
  case air_action::withdraw:
    {
      auto const target{actor.target};
      actor.actions >>= 8;
      if(target && *target != player_air_target && (flagged(*actors[*target],air_flag::crashed) || flagged(*actors[*target],air_flag::retired))) {
        actor.target.reset();
        push(actor,air_action::normal);
        return false;
      }
      if(actor.wait_seconds >= 0) { push(actor,air_action::engagement); return false; }
      actor.wait_seconds = word(actor.wait_seconds + 1);
      if(acquire(index,target,world)) { actor.wait_seconds = 0; return false; }
      reset_air_controls(state);
      command_pitch(state,actor.performance,static_cast<math::angle>(altitude_pitch(state,actor.performance,pose.position.y)),context);
      auto const destination{contact(target.value(),world)};
      cruise_turn(word(math::bearing(pose.position,destination.position).y - 32768 - pose.angles.y));
      push(actor,air_action::withdraw);
      return true;
    }
  case air_action::engagement:
    {
      actor.actions >>= 8;
      reset_air_controls(state);
      auto const target{contact(actor.target.value(),world)};
      int32_t const distance{horizontal_distance(pose.position,target.position)};
      auto const angles{math::bearing(pose.position,target.position)};
      int16_t const heading{word(angles.y - pose.angles.y)};
      int16_t const reciprocal{word(angles.y + 32768 - target.angles.y)};
      uint16_t flags{world.combat_flags};
      bool const close{distance < actor.combat.gun_range * fixed_units_per_foot};
      flags |= close ? 16 : 32;
      auto const classify{[](int16_t const angle){
        if(angle == -32768) return 2u;
        int const magnitude{std::abs(static_cast<int>(angle))};
        return magnitude < nose_capture_angle ? 1u : magnitude > tail_capture_angle ? 2u : 0u;
      }};
      flags |= static_cast<uint16_t>(classify(heading) | (classify(reciprocal) << 2));
      bool const on_nose{(flags & 1) != 0}, target_on_nose{(flags & 4) != 0}, target_tail{(flags & 8) != 0};
      if(on_nose) {
        actor.wait_seconds = word(actor.wait_seconds + 1);
        if(actor.wait_seconds > 1 && actor.target == player_air_target) requests.push_back({
          .actor{index},
          .data{air_warning{
            .target{player_air_target},
            .kind{air_warning_kind::tracking},
          }},
        });
        bool const player_target{actor.target == player_air_target};
        bool const allowed{!player_target || actor.player_shots < actor.combat.maximum_player_shots};
        bool const missile{(flags & 32) && allowed && actor.wait_seconds > actor.combat.firing_delay};
        bool const gun{!(flags & 32) && (flags & 16)};
        if(world.weapons_enabled && (missile || gun)) {
          requests.push_back({
            .actor{index},
            .data{air_warning{
              .target{actor.target.value()},
              .kind{air_warning_kind::firing},
            }},
          });
          requests.push_back({
            .actor{index},
            .data{air_weapon{
              .position{vec3<int32_t>{pose.position}},
              .angles{gun ? angles.x : pose.angles.x,gun ? angles.y : pose.angles.y,0},
              .speed{word(pose.speed >> speed_fraction_bits)},
              .weapon{static_cast<uint16_t>(gun ? 0 : target_tail ? 3 : 4)},
              .target{vec3<int32_t>{target.position}},
              .target_actor{actor.target},
            }},
          });
          if(missile) {
            actor.wait_seconds = word(-actor.combat.firing_cooldown);
            if(player_target) ++actor.player_shots;
          }
        }
      } else actor.wait_seconds = 0;
      if(target_on_nose && on_nose) cruise_turn(word(-2 * heading));
      else if(target_on_nose) {
        if(choose(1)) return false;
        auto const choice{random.next() % 4};
        int32_t const height{choice == 0 ? pose.position.y : wrap(static_cast<int64_t>(target.position.y) + (choice == 2 ? -3200 : choice == 3 ? 3200 : 0))};
        command_pitch(state,actor.performance,static_cast<math::angle>(altitude_pitch(state,actor.performance,height)),context);
        cruise_turn(heading);
      } else {
        if(!on_nose && !target_tail && choose(0)) return false;
        shorten_air_step(state,distance);
        command_pitch(state,actor.performance,static_cast<math::angle>(altitude_pitch(state,actor.performance,wrap(static_cast<int64_t>(target.position.y) - 3200))),context);
        int32_t speed{wrap(static_cast<int64_t>(target.speed) + static_cast<int64_t>(difference(distance,actor.combat.gun_range * fixed_units_per_foot) / 3200) * (1 << speed_fraction_bits))};
        if(command_turn(state,actor.performance,heading,context)) speed = static_cast<int32_t>(cruise_speed(state,actor.performance,envelope)) << speed_fraction_bits;
        command_speed(state,actor.performance,speed);
      }
      push(actor,air_action::withdraw);
      return true;
    }
  default: throw std::runtime_error{std::format("unimplemented aircraft combat action {}",action)};
  }
}

std::vector<air_request> air_runtime::advance(air_context const &context) {
  std::vector<air_request> requests;
  request_queue output{requests,context};
  // Original 0x75784 visits the other/hostile air pool, then the allied pool; followers observe their leader's updated state.
  for(unsigned int index{0}; index != actors.size(); ++index) if(actors[index] && !flagged(*actors[index],air_flag::retired)) advance_actor(index,context,output);
  return requests;
}

void air_runtime::advance_actor(unsigned int const index, air_context const &world, request_queue &requests) {
  auto &actor{*actors[index]};
  auto &state{actor.flight};
  auto &pose{state.pose};
  air_control_context const context{
    .model{world.model},
    .turn_envelope{envelope},
  };
  auto const emit{[&](auto data){
    requests.emplace(index,std::move(data));
  }};
  auto const visual{[&](unsigned int const entry){ emit(air_scenery_event{.entry{entry}}); }};
  auto const launch{[&](vec3<int32_t> const &target, uint16_t const weapon){
    int const jitter{std::clamp(static_cast<int>(math::signed_word(random.next())),-182,182)};
    emit(air_weapon{
      .position{vec3<int32_t>{pose.position}},
      .angles{pose.angles.x,static_cast<math::angle>(pose.angles.y + jitter),0},
      .speed{math::signed_word(static_cast<uint16_t>(pose.speed >> speed_fraction_bits))},
      .weapon{weapon},
      .target{vec3<int32_t>{target}},
    });
  }};
  if(state.input_ticks <= 0) {
    bool ready{false};
    for(unsigned int step{0}; step != controller_budget && !ready; ++step) {
      auto const mode{static_cast<air_action>(std::min(actor.actions & 255,12u))};
      if(mode == air_action::clear) { actor.actions = 0; continue; }
      if(mode == air_action::retire_ejected || mode == air_action::retire_crashed) {
        actor.actions = 0;
        set_flag(actor,air_flag::retired);
        return;
      }
      if(mode == air_action::waiting) {
        if(state.input_ticks < 0) { ++state.input_ticks; return; }
        if(actor.wait_seconds > 0) { state.input_ticks = -air_control_horizon; --actor.wait_seconds; return; }
        actor.actions >>= 8;
        continue;
      }
      if(mode == air_action::finish_takeoff) {
        command_pitch(state,actor.performance,0,context);
        actor.actions >>= 8;
        continue;
      }
      if(mode == air_action::normal) {
        actor.actions >>= 8;
        reset_air_controls(state);
        set_flag(actor,air_flag::afterburner,pose.speed >= static_cast<int32_t>(actor.performance.afterburner_speed) << speed_fraction_bits);
        emit(air_target_scan{});
        if(world.combat && acquire(index,{},world)) continue;
        if(follow_formation(actor,context)) ready = true;
        continue;
      }
      if(mode != air_action::route && world.combat) {
        ready = combat_action(index,std::to_underlying(mode),world,requests);
        if(mode == air_action::falling && actor.actions == std::to_underlying(air_action::retire_crashed)) return;
        continue;
      }
      if(mode != air_action::route) throw std::runtime_error{std::format("aircraft {} (profile {}, record {}, byte {}) needs combat/manoeuvre action {}",index,pose.profile_id,actor.location.record,actor.location.address,std::to_underlying(mode))};
      auto &bytes{commands.at(actor.location.record)};
      resources::binary_view const code{bytes};
      auto const pc{actor.location.address};
      auto const opcode{static_cast<command>(code.byte(pc))};
      auto const operand{[&](unsigned int const n){ return code.le16(pc + 1 + n * 2); }};
      auto const branch{[&](int const amount){
        int64_t const target{static_cast<int64_t>(actor.location.address) + amount};
        if(target < 0 || static_cast<uint64_t>(target) >= bytes.size()) throw std::runtime_error{"air waypoint branch leaves its program"};
        actor.location.address = static_cast<unsigned int>(target);
      }};
      auto const store{[&](unsigned int const address, int32_t const value){
        for(unsigned int byte{0}; byte != 4; ++byte) bytes.at(address + byte) = static_cast<uint8_t>(static_cast<uint32_t>(value) >> (byte * 8));
      }};
      auto const destination{[&](unsigned int const first){
        auto result{cell_position(math::signed_word(operand(first)),math::signed_word(operand(first + 1)),pose.position.y)};
        if(pose.formation) result = formation_position(result,pose.angles.y,pose.formation);
        return result;
      }};
      switch(opcode) {
      case command::end: return;
      case command::aircraft_initial_state: code.slice(pc,25); push(actor,air_action::normal); branch(25); break;
      case command::reserved_7: code.slice(pc,11); branch(11); break;
      case command::reserved_8: code.slice(pc,9); branch(9); break;
      case command::speed:
        command_speed(state,actor.performance,std::bit_cast<int32_t>(static_cast<uint32_t>(operand(0)) << speed_fraction_bits));
        branch(3);
        break;
      case command::altitude:
        // Native 0x77ba0 snaps within 91 angle units and otherwise omits command_pitch's small-error adjustment.
        {
          int32_t const height{wrap(static_cast<int64_t>(formation[pose.formation].height) - static_cast<int64_t>(operand(0)) * fixed_units_per_foot)};
          int16_t const target_pitch{altitude_pitch(state,actor.performance,height)};
          int16_t const error{word(target_pitch - pose.angles.x)};
          if(error != -32768 && std::abs(static_cast<int>(error)) < 91) { state.angular_rate.x = 0; pose.angles.x = static_cast<math::angle>(pose.angles.x + error); } else {
            int const maximum{maximum_turn_rate(state,actor.performance,context)};
            state.angular_rate.x = static_cast<int16_t>(std::clamp(error / air_control_horizon,-maximum,maximum));
          }
          if(state.input_ticks > air_control_horizon) state.input_ticks = air_control_horizon;
          branch(3);
        }
        break;
      case command::move_until_near: case command::waypoint_branch:
        {
          push(actor,air_action::normal);
          bool const near_command{opcode == command::move_until_near};
          auto const target{destination(near_command ? 0 : 1)};
          int32_t const distance{horizontal_distance(pose.position,target)};
          bool const near{distance < waypoint_near_radius};
          if(near) pose.flags |= static_cast<uint16_t>(burst_mask | std::to_underlying(air_flag::new_waypoint));
          if(near_command) branch(near ? 7 : -math::signed_word(operand(2)));
          else branch(-math::signed_word(operand(near ? 3 : 0)));
          if(!flagged(actor,air_flag::waypoint_steering)) {
            if(!near_command || !near) course(actor,target,distance,false,context);
          }
          ready = !near_command || !near || flagged(actor,air_flag::waypoint_steering);
        }
        break;
      case command::pursue_waypoint:
        {
          set_flag(actor,air_flag::waypoint_steering);
          auto const target{destination(1)};
          int32_t const distance{horizontal_distance(pose.position,target)};
          if(distance > static_cast<int32_t>(operand(0)) * fixed_units_per_mile) course(actor,target,distance,true,context);
          branch(7);
        }
        break;
      case command::wait:
        if(flagged(actor,air_flag::awaiting_takeoff)) {
          set_flag(actor,air_flag::awaiting_takeoff,false);
          actor.wait_seconds = word(math::signed_word(operand(0)) * 60 + math::signed_word(operand(1)));
          push(actor,air_action::waiting);
        }
        branch(5);
        break;
      case command::radio_message:
        if(!operand(0)) {
          auto const text{code.slice(pc + 3,40)};
          auto const end{std::ranges::find(text,uint8_t{0})};
          emit(air_radio{.text{text.begin(),end}});
          bytes.at(pc + 1) = 1;
          bytes.at(pc + 2) = 0;
        }
        branch(43);
        break;
      case command::takeoff:
        if(flagged(actor,air_flag::airborne)) { branch(7); break; }
        if(!pose.speed) {
          pose.angles = vec3<math::angle>{0,static_cast<math::angle>(operand(0) * 182),0};
          state.acceleration = std::bit_cast<int32_t>(static_cast<uint32_t>(operand(1)) << speed_fraction_bits) / air_control_horizon;
          state.acceleration |= 1;
          set_flag(actor,air_flag::afterburner);
        }
        if(pose.speed >= std::bit_cast<int32_t>(static_cast<uint32_t>(operand(2)) << speed_fraction_bits)) {
          state.angular_rate.x = -18;
          state.input_ticks = air_control_horizon;
          branch(7);
          set_flag(actor,air_flag::airborne);
          push(actor,air_action::finish_takeoff);
          push(actor,air_action::normal);
        } else {
          if(!state.acceleration) throw std::runtime_error{"air takeoff has zero acceleration"};
          int const ticks{difference(std::bit_cast<int32_t>(static_cast<uint32_t>(operand(2)) << speed_fraction_bits),pose.speed) / state.acceleration};
          state.input_ticks = static_cast<int16_t>(ticks < 1 || ticks > air_control_horizon ? air_control_horizon : ticks);
        }
        ready = true;
        break;
      case command::attack_ground:
        if(flagged(actor,air_flag::new_waypoint)) {
          set_flag(actor,air_flag::new_waypoint,false);
          vec2<int16_t> const cell{math::signed_word(operand(0)),math::signed_word(operand(1))};
          if(world.intact_ground_target && world.intact_ground_target(cell)) {
            auto const target{cell_position(cell.x,cell.y,0)};
            store(pc + 5,target.x); store(pc + 9,target.y); store(pc + 13,target.z);
            // This opcode launches without dispersion; its release angles and target are explicit outputs.
            emit(air_weapon{
              .position{vec3<int32_t>{pose.position}},
              .angles{pose.angles.x,pose.angles.y,0},
              .speed{math::signed_word(static_cast<uint16_t>(pose.speed >> speed_fraction_bits))},
              .weapon{operand(8)},
              .target{vec3<int32_t>{target}},
            });
          }
        }
        branch(19);
        break;
      case command::conditional_action:
        {
          vec2<int16_t> const cell{math::signed_word(operand(0)),math::signed_word(operand(1))};
          if(world.intact_ground_target && world.intact_ground_target(cell)) {
            auto const target{cell_position(cell.x,cell.y,0)};
            store(pc + 5,target.x); store(pc + 9,target.y); store(pc + 13,target.z);
            if(horizontal_distance(pose.position,target) < fixed_units_per_mile) {
              auto const heading{math::signed_word(math::bearing(pose.position,target).y)};
              if(heading > -16383 && heading < 16383) launch(target,8);
            }
          }
          branch(17);
        }
        break;
      case command::conditional_action_12:
        {
          unsigned int remaining{static_cast<unsigned int>(pose.flags) >> burst_shift};
          if(remaining == 63) {
            remaining = std::min<unsigned int>(operand(3),62);
            if(remaining && code.byte(pc + 11)) visual(code.byte(pc + 11));
            pose.flags = static_cast<uint16_t>((pose.flags & ~burst_mask) | (remaining << burst_shift));
          }
          if(remaining) {
            auto const target{cell_position(math::signed_word(operand(0)),math::signed_word(operand(1)),0)};
            store(pc + 13,target.x); store(pc + 17,target.y); store(pc + 21,target.z);
            if(horizontal_distance(pose.position,target) < static_cast<int32_t>(operand(4)) * fixed_units_per_foot) {
              auto const heading{math::signed_word(math::bearing(pose.position,target).y)};
              if(heading > -16383 && heading < 16383) {
                launch(target,operand(2));
                --remaining;
                pose.flags = static_cast<uint16_t>((pose.flags & ~burst_mask) | (remaining << burst_shift));
                if(!remaining && code.byte(pc + 12)) visual(code.byte(pc + 12));
              }
            }
          }
          branch(25);
        }
        break;
      default: return;
      }
    }
    if(!ready) throw std::runtime_error{"air route does not yield within its instruction budget"};
  }
  if(auto const event{advance_air_motion(state)}) visual(std::to_underlying(*event));
}

} // namespace tfx::actors
