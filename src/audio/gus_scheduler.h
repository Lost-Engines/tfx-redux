#pragma once

#include <array>
#include <optional>
#include "audio/sample_scheduler.h"

namespace tfx::audio {

struct gus_sample_descriptor {
  sample_descriptor sample;
  bool resident{false};
};
struct gus_scheduler_state {
  bool enabled{true};
  uint8_t priority{0};
  uint8_t resident_cursor{1};                                                  // Native DFC1 starts at one; the first resident request selects voice two.
};
struct gus_request {
  unsigned int voice;
  std::vector<unsigned int> sequence;
};

// Native DDADC reserves voice zero for sequences containing streamed samples.
// Entirely resident sequences rotate through voices 1..3. The driver supplies
// stopped flags; accepted requests always stop and replace the selected queue.
class gus_scheduler {
private:
  std::span<gus_sample_descriptor const> definitions;
  std::vector<int16_t> cooldown;
  gus_scheduler_state state;

public:
  static unsigned int constexpr voice_count{4}, stream_voice{0}, first_resident_voice{1};
  static unsigned int constexpr voice_queue_capacity{30};
  explicit gus_scheduler(std::span<gus_sample_descriptor const> definitions, gus_scheduler_state initial = {});
  void enable(bool enabled);
  void advance(simulation::timer_ticks elapsed);
  std::optional<gus_request> submit(sample_priority priority, std::span<unsigned int const> sequence,
    std::array<bool,voice_count> const &stopped);
  gus_scheduler_state const &value() const;
  std::span<int16_t const> remaining() const;
};

} // namespace tfx::audio
