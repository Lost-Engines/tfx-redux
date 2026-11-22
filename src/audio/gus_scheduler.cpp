#include "gus_scheduler.h"
#include <bit>
#include <stdexcept>
#include <utility>

namespace tfx::audio {

gus_scheduler::gus_scheduler(std::span<gus_sample_descriptor const> const definitions, gus_scheduler_state const initial)
  : definitions{definitions},
    cooldown(definitions.size(), 0),
    state{initial} {
  if(state.resident_cursor >= voice_count) throw std::out_of_range{"Gravis resident voice cursor exceeds its pool"};
}
void gus_scheduler::enable(bool const enabled) {
  state.enabled = enabled;
}
void gus_scheduler::advance(simulation::timer_ticks const elapsed) {
  for(auto &remaining : cooldown) {
    if(remaining > 0) {
      remaining = std::bit_cast<int16_t>(static_cast<uint16_t>(remaining - elapsed));
      if(remaining < 0) remaining = 0;
    }
  }
}
std::optional<gus_request> gus_scheduler::submit(sample_priority const priority, std::span<unsigned int const> const sequence,
    std::array<bool, voice_count> const &stopped) {
  if(!state.enabled || sequence.empty()) return {};
  if(sequence.size() > voice_queue_capacity) throw std::out_of_range{"Gravis request exceeds its voice queue"};
  bool resident{true};
  // Unlike the SB stream path, native DE068 rejects any missing item before
  // changing the first cooldown, stopping a voice or rotating the cursor.
  for(auto const index : sequence) {
    auto const &definition{definitions.at(index)};
    if(!definition.sample.available) return {};
    resident = resident && definition.resident;
  }
  auto const first{sequence.front()};
  if(cooldown[first]) return {};
  unsigned int voice{stream_voice};
  if(resident) {
    if(++state.resident_cursor == voice_count) state.resident_cursor = first_resident_voice;
    voice = state.resident_cursor;
  }
  auto const level{std::to_underlying(priority)};
  // A rejected busy resident voice still advances the rotating cursor.
  if(!stopped[voice] && (level < state.priority || !level)) return {};
  cooldown[first] = definitions[first].sample.retrigger_delay;
  state.priority = level;
  gus_request request{
    .voice{voice},
    .sequence{},
  };
  for(auto const index : sequence) {
    if(definitions[index].sample.samples) request.sequence.push_back(index);
  }
  return request;
}
gus_scheduler_state const &gus_scheduler::value() const {
  return state;
}
std::span<int16_t const> gus_scheduler::remaining() const {
  return cooldown;
}

} // namespace tfx::audio
