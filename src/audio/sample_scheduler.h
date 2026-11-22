#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "simulation/timing.h"

namespace tfx::audio {

enum class sample_priority : uint8_t { background, normal, urgent };
struct sample_descriptor {
  int16_t retrigger_delay{0};                                                  // Native signed word; -1 prevents subsequent requests.
  uint32_t samples{0};
  bool available{false};
};
struct sample_scheduler_state {
  bool enabled{true};
  bool active{false};
  uint8_t priority{0};
};
struct sample_request {
  bool interrupted{false};
  bool started{false};
  std::vector<unsigned int> sequence{};
};

// The standard Sound Blaster path plays one concatenated stream. A request
// applies priority/cooldown to its first descriptor, then queues the rest in
// order. Resident resource storage and hardware completion belong to the caller.
class sample_scheduler {
private:
  std::span<sample_descriptor const> definitions;
  std::vector<int16_t> cooldown;
  sample_scheduler_state state;

public:
  explicit sample_scheduler(std::span<sample_descriptor const> definitions, sample_scheduler_state initial = {});
  void advance(simulation::timer_ticks elapsed);
  sample_request submit(sample_priority priority, std::span<unsigned int const> sequence);
  void complete();
  void enable(bool enabled);
  sample_scheduler_state const &value() const;
  std::span<int16_t const> remaining() const;
};

} // namespace tfx::audio
