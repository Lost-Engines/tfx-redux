#include "timeline.h"
#include <algorithm>

namespace tfx::missions {

timeline::timeline(setup const &source)
  : initialisation{source},
    delivered_messages(source.messages.size()),
    dispatched_events(source.scenery_events.size()) {
}

void timeline::reset(mission_tick const elapsed) {
  elapsed_ticks = elapsed;
  std::ranges::fill(delivered_messages, 0);
  std::ranges::fill(dispatched_events, 0);
}

std::vector<unsigned int> timeline::begin_frame() {
  /// Native 0x71b40 runs before flight/actors and includes messages exactly at the current unsigned mission tick.
  std::vector<unsigned int> result;
  for(unsigned int index{0}; index != delivered_messages.size(); ++index) {
    if(!delivered_messages[index] && static_cast<mission_tick>(initialisation.messages[index].time) <= elapsed_ticks) {
      delivered_messages[index] = 1;
      result.push_back(index);
    }
  }
  return result;
}

void timeline::advance_clock(simulation::timer_ticks const elapsed) {
  // Native 0x72324 advances mission time before drawing the HUD and its clock.
  elapsed_ticks += elapsed;
}

std::vector<unsigned int> timeline::finish_frame() {
  // Native 0x70e44 runs after the HUD: scenery events strictly before this tick, in authored order.
  std::vector<unsigned int> result;
  for(unsigned int index{0}; index != dispatched_events.size(); ++index) {
    if(!dispatched_events[index] && static_cast<mission_tick>(initialisation.scenery_events[index].time) < elapsed_ticks) {
      dispatched_events[index] = 1;
      result.push_back(index);
    }
  }
  return result;
}

mission_tick timeline::elapsed() const {
  return elapsed_ticks;
}

mission_tick timeline::time_of_day() const {
  return elapsed_ticks + static_cast<mission_tick>(initialisation.clock);
}

} // namespace tfx::missions
