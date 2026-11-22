#pragma once

#include <algorithm>
#include <stdexcept>
#include "simulation/timing.h"

namespace tfx::audio {

// TFX's START wrapper disables MEDIT's private timer. The shared game ISR
// ticks OSL once per PIT 0x5555 interrupt, regardless of a song's rate field.
class music_clock {
private:
  uint64_t phase{0};
  uint64_t const interval;

public:
  explicit music_clock(unsigned int const output_rate) : interval{static_cast<uint64_t>(output_rate) * simulation::pit_divisor} {
    if(!output_rate) throw std::invalid_argument{"music output rate must not be zero"};
  }
  bool due() const {
    return phase >= interval;
  }
  void consume() {
    if(!due()) throw std::logic_error{"music interrupt is not due"};
    phase -= interval;
  }
  unsigned int until_tick() const {
    return due() ? 0 : static_cast<unsigned int>((interval - phase + simulation::pit_input_hz - 1) / simulation::pit_input_hz);
  }
  void advance(unsigned int const frames) {
    phase += static_cast<uint64_t>(frames) * simulation::pit_input_hz;
  }
};

} // namespace tfx::audio
