#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include "audio/sample_bank.h"

namespace tfx::audio {

struct stereo_frame {
  float left{};
  float right{};
};

// The recovered two-input model receives the mono Sound Blaster stream only.
// Its firmware bank is independent of the DAC time constant. MIDI/CD bypass it.
class qsound_effect {
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  qsound_effect(std::span<uint8_t const> firmware, unsigned int output_rate);
  ~qsound_effect();
  void select(sample_pan const &parameters);
  void advance_frame();
  stereo_frame process(uint8_t sample) noexcept;
  unsigned int tail_samples() const noexcept;
};

// Loading-thread lookup; accepts a standalone CSP or the fetched MAIN.PVL.
std::unique_ptr<qsound_effect> load_qsound_effect(std::span<std::filesystem::path const> directories,
  unsigned int output_rate);

} // namespace tfx::audio
