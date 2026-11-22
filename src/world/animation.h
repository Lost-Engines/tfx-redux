#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "simulation/timing.h"

namespace tfx::world {

using simulation::timer_ticks;
using simulation::timer_hz;
timer_ticks constexpr animation_interval{7}, maximum_animation_backlog{100};

enum class animation_policy {
  unconditional,
  when_visible,
  frozen
};
constexpr bool animation_eligible(animation_policy const policy, bool const enabled, bool const visible) {
  return enabled && policy != animation_policy::frozen && (policy == animation_policy::unconditional || visible);
}

enum class stream_action {
  value_written,
  finished,
  event_requested
};
struct stream_step {
  stream_action action;
  int16_t event{0};
};

class parameter_player {
private:
  std::vector<uint16_t> words;
  unsigned int cursor{0};
  bool finished{false};

public:
  parameter_player(std::span<uint16_t const> source);
  stream_step tick(uint8_t &parameter);
  unsigned int position() const;
  bool stopped() const;
};

class animation_clock {
private:
  timer_ticks accumulated{0};

public:
  bool advance(timer_ticks elapsed);
  int16_t remainder() const;
};

} // namespace tfx::world
