#pragma once

#include <array>
#include <memory>
#include "audio/gus_driver.h"
#include "audio/gus_scheduler.h"

namespace tfx::audio {

struct gus_sample_data {
  std::span<uint8_t const> pcm;
  uint16_t rate;
  int16_t retrigger_delay;
};
struct gus_playback_state {
  gus_scheduler_state scheduling;
  std::array<unsigned int, gus_scheduler::voice_count> pending;
  std::array<unsigned int, gus_scheduler::voice_count> started;
  std::vector<int16_t> cooldowns;
};

// TFX's resident four-voice effects path on a one-MiB GF1. PCM storage must
// outlive this object. Host requests and the audio callback share a bounded lock.
class gus_samples {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  gus_samples(std::span<gus_sample_data const> samples, gus_parameters const &parameters, unsigned int output_rate);
  ~gus_samples();
  void enable(bool enabled);
  void advance(simulation::timer_ticks elapsed);
  void submit(sample_priority priority, std::span<unsigned int const> sequence);
  void render(std::span<float> stereo);
  gus_playback_state value();
};

} // namespace tfx::audio
