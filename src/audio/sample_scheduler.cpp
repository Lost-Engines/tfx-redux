#include "sample_scheduler.h"
#include <bit>
#include <stdexcept>
#include <utility>

namespace tfx::audio {

sample_scheduler::sample_scheduler(std::span<sample_descriptor const> const definitions, sample_scheduler_state const initial)
  :
  definitions{definitions},
    cooldown(definitions.size(), 0),
    state{initial} {}
void sample_scheduler::advance(simulation::timer_ticks const elapsed) {
  // Native 0xc513c subtracts in the original word before its signed clamp.
  for(auto &remaining : cooldown) {
    if(remaining > 0) {
      remaining = std::bit_cast<int16_t>(static_cast<uint16_t>(remaining - elapsed));
      if(remaining < 0) remaining = 0;
    }
  }
}
sample_request sample_scheduler::submit(sample_priority const priority, std::span<unsigned int const> const sequence) {
  sample_request result;
  if(!state.enabled || sequence.empty()) return result;
  auto const level{std::to_underlying(priority)};
  if(state.active && (level < state.priority || !level)) return result;
  auto const first{sequence.front()};
  if(first >= definitions.size()) throw std::out_of_range{"sample index exceeds the loaded sound bank"};
  if(cooldown[first] || !definitions[first].available) return result;
  cooldown[first] = definitions[first].retrigger_delay;
  result.interrupted = state.active;
  state.active = false;
  for(auto const index : sequence) {
    if(index >= definitions.size()) throw std::out_of_range{"sample index exceeds the loaded sound bank"};
    // A missing later item invalidates the complete concatenation, after the
    // existing stream has stopped and the first cooldown has been assigned.
    if(!definitions[index].available) {
      result.sequence.clear();
      return result;
    }
    if(definitions[index].samples) result.sequence.push_back(index);
  }
  state.active = result.started = !result.sequence.empty();
  state.priority = level;
  return result;
}
void sample_scheduler::complete() {
  state.active = false;
}
void sample_scheduler::enable(bool const enabled) {
  state.enabled = enabled;
}
sample_scheduler_state const &sample_scheduler::value() const {
  return state;
}
std::span<int16_t const> sample_scheduler::remaining() const {
  return cooldown;
}

} // namespace tfx::audio
