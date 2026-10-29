#include "surface.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "math/attitude.h"
#include "world/environment.h"

namespace tfx::actors {
namespace {

int constexpr input_horizon{55}, waypoint_near_distance{32000};
unsigned int constexpr vm_budget{1024};
uint16_t constexpr suppress_steering{1}, new_wait{4};
int32_t wrap(int64_t const value) { return std::bit_cast<int32_t>(static_cast<uint32_t>(value)); }
int16_t word(int const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t absolute(int32_t const value) { return value < 0 ? wrap(-static_cast<int64_t>(value)) : value; }
void push(surface_state &state, uint8_t const mode) {
  if(!(state.actions >> 24)) state.actions = (state.actions << 8) | mode;
}
void reduce_ticks(surface_state &state, int const ticks) {
  if(ticks > 0 && ticks < state.input_ticks) state.input_ticks = static_cast<int16_t>(ticks);
}
math::angle bearing(int32_t x, int32_t z) {
  return math::direction_angle(x,z);
}
void course(surface_state &state, profile const &profile, vec3<int32_t> const &destination, int32_t const distance) {
  resources::binary_view const parameters{profile.source};
  auto const desired{bearing(wrap(static_cast<int64_t>(destination.x) - state.pose.position.x),wrap(static_cast<int64_t>(destination.z) - state.pose.position.z))};
  int const delta{word(desired - state.pose.angles.y)};
  int const maximum{parameters.signed_le16(40)};
  state.heading_rate = static_cast<int16_t>(std::clamp(delta / input_horizon,-maximum,maximum));
  reduce_ticks(state,input_horizon);
  int const ticks{distance / ((state.pose.speed >> speed_fraction_bits) | 1)};
  reduce_ticks(state,ticks < 1 || ticks > input_horizon ? input_horizon : ticks);
}
void speed(surface_state &state, profile const &profile, uint16_t const requested) {
  resources::binary_view const parameters{profile.source};
  int32_t const maximum{std::bit_cast<int32_t>(static_cast<uint32_t>(parameters.le16(38)) << speed_fraction_bits)};
  int32_t const target{std::clamp(std::bit_cast<int32_t>(static_cast<uint32_t>(requested) << speed_fraction_bits),int32_t{0},maximum)};
  int32_t const acceleration{std::bit_cast<int32_t>(parameters.le32(34))};
  state.acceleration = std::clamp(wrap(static_cast<int64_t>(target) - state.pose.speed) / input_horizon,-acceleration,acceleration);
  reduce_ticks(state,input_horizon);
}
void branch(surface_state &state, profile const &profile, int16_t const x, int16_t const z, int16_t const near_jump, int16_t const far_jump) {
  push(state,3);
  vec3<int32_t> const destination{
    x * world::cell_units * position_unit + world::cell_units / 2 * position_unit,
    state.pose.position.y,
    z * world::cell_units * position_unit + world::cell_units / 2 * position_unit,
  };
  int32_t const distance{std::max(absolute(wrap(static_cast<int64_t>(destination.x) - state.pose.position.x)),absolute(wrap(static_cast<int64_t>(destination.z) - state.pose.position.z)))};
  auto const jump{distance < waypoint_near_distance ? near_jump : -static_cast<int>(far_jump)};
  state.cursor = static_cast<unsigned int>(static_cast<int64_t>(state.cursor) + jump);
  if(!(state.pose.flags & suppress_steering)) course(state,profile,destination,distance);
}

} // anonymous namespace

surface_state create_surface(initial_actor const &actor, program const &source) {
  if(actor.kind != record_kind::surface) throw std::invalid_argument{"surface controller requires a surface actor"};
  return {
    .pose{actor},
    .commands{source.records.at(actor.record).body},
  };
}

std::vector<surface_request> advance_surface(surface_state &state, profile const &profile, std::function<void(surface_request const&)> emitted) {
  /// Native 0x7c2bc executes one AI tick; live target/weapon requests remain explicit outputs
  std::vector<surface_request> requests;
  auto const emit{[&](surface_request request) {
    requests.push_back(std::move(request));
    if(emitted) emitted(requests.back());
  }};
  if(state.input_ticks <= 0) {
    bool ready{false};
    for(unsigned int step{0}; step != vm_budget && !ready; ++step) {
      auto const action{state.actions & 255};
      if(action == 1 || action == 2) {
        if(action == 2) { state.pose.speed = 0; state.input_ticks = 120; state.actions = 1; }
        return requests;
      }
      if(action == 3) {
        state.actions >>= 8;
        state.heading_rate = 0;
        state.acceleration = 0;
        state.input_ticks = input_horizon;
        state.pose.flags &= static_cast<uint16_t>(~suppress_steering);
        emit({.kind{surface_request_kind::nearby_targets}});
        continue;
      }
      if(action == 4) {
        if(state.input_ticks < 0) { ++state.input_ticks; return requests; }
        if(state.wait_seconds > 0) {
          state.input_ticks = -input_horizon;
          --state.wait_seconds;
          return requests;
        }
        state.actions >>= 8;
        continue;
      }
      if(action > 4) { state.actions = 0; continue; }
      resources::binary_view const code{state.commands};
      auto const opcode{static_cast<command>(code.byte(state.cursor))};
      auto const operand{[&](unsigned int const index){ return code.le16(state.cursor + 1 + index * 2); }};
      switch(opcode) {
      case command::surface_initial_state: push(state,3); state.cursor += 13; break;
      case command::speed: speed(state,profile,operand(0)); state.cursor += 3; break;
      case command::move_until_near: branch(state,profile,math::signed_word(operand(0)),math::signed_word(operand(1)),7,math::signed_word(operand(2))); ready = true; break;
      case command::waypoint_branch: branch(state,profile,math::signed_word(operand(1)),math::signed_word(operand(2)),word(-math::signed_word(operand(3))),math::signed_word(operand(0))); ready = true; break;
      case command::wait:
        if(state.pose.flags & new_wait) {
          state.pose.flags &= static_cast<uint16_t>(~new_wait);
          state.wait_seconds = word(math::signed_word(operand(0)) * 60 + math::signed_word(operand(1)));
          push(state,4);
        }
        state.cursor += 5;
        break;
      case command::radio_message:
        if(!operand(0)) {
          auto const text{code.slice(state.cursor + 3,40)};
          auto const end{std::ranges::find(text,uint8_t{0})};
          emit({
            .kind{surface_request_kind::radio},
            .text{text.begin(),end},
          });
          state.commands[state.cursor + 1] = 1;
        }
        state.cursor += 43;
        break;
      case command::attack_ground:
        emit({
          .kind{surface_request_kind::ground_target},
          .cell{math::signed_word(operand(0)),math::signed_word(operand(1))},
          .weapon{operand(8)},
        });
        state.cursor += 19;
        break;
      case command::reserved_7: code.slice(state.cursor,11); state.cursor += 11; break;
      default: return requests;                                               // native dispatcher returns -1 without advancing unsupported commands, including END
      }
    }
    if(!ready) throw std::runtime_error{"surface route does not yield within its instruction budget"};
  }
  state.input_ticks = word(state.input_ticks - 1);
  state.pose.speed = wrap(static_cast<int64_t>(state.pose.speed) + state.acceleration);
  state.pose.angles.y = static_cast<math::angle>(state.pose.angles.y + state.heading_rate);
  move(state.pose.position,state.pose.angles,state.pose.speed >> speed_fraction_bits);
  return requests;
}

} // namespace tfx::actors
