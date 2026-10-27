#pragma once

#include <mutex>
#include "audio/sample_bank.h"
#include "audio/qsound_effect.h"

namespace tfx::audio {

// Device-independent single-stream DAC. The bank must outlive playback.
// Host requests and the device callback share this small, bounded lock.
class sample_voice {
private:
  sample_bank const &bank;
  sample_scheduler scheduler;
  std::mutex mutex;
  std::vector<unsigned int> queue;
  unsigned int item{0}, offset{0};
  uint64_t phase{0};
  uint32_t const output_rate;
  std::unique_ptr<qsound_effect> spatial;
  uint16_t source_period{125};
  unsigned int tail_remaining{};
  stereo_frame held{};
  bool latch_pending{};
  void select_item();

public:
  explicit sample_voice(sample_bank const &bank, uint32_t output_rate, std::unique_ptr<qsound_effect> spatial = {});
  void enable(bool enabled);
  void advance(simulation::timer_ticks elapsed);
  void submit(sample_priority priority, std::span<unsigned int const> sequence);
  // Preserve DMA holds and fractional boundaries across callbacks. Optional
  // QSound processes each DAC transition, with bounded silence-tail drainage.
  void render(std::span<float> stereo);
  sample_scheduler_state state();
};

} // namespace tfx::audio
