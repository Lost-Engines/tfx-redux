#pragma once

#include <memory>
#include "audio/sample_bank.h"

namespace tfx::audio {

struct stereo_frame {
  float left{};
  float right{};
};

// The recovered two-input model receives the mono Sound Blaster stream only.
// Its built-in 8 kHz bank is independent of the DAC time constant. MIDI/CD bypass it.
class qsound_effect {
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  explicit qsound_effect(unsigned int output_rate);
  ~qsound_effect();
  void select(sample_pan const &parameters);
  void advance_frame();
  stereo_frame process(uint8_t sample) noexcept;
  unsigned int tail_samples() const noexcept;
};

} // namespace tfx::audio
