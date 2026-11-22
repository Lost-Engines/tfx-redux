#pragma once

#include <memory>
#include "audio/midi_receiver.h"
#include "audio/osl_device.h"

namespace tfx::audio {

// Pair the original device callbacks with the hardware that consumes their
// output. The player owns timing and song state independently of either.
class music_device {
public:
  virtual ~music_device() = default;
  virtual osl_device_driver &driver() = 0;
  virtual void render(std::span<float> stereo) = 0;
  virtual unsigned int reset_settling_frames(unsigned int sample_rate) const {
    return sample_rate / 10;
  }
};

std::unique_ptr<music_device> make_fm_music_device(std::span<uint8_t const> launcher, unsigned int sample_rate);
std::unique_ptr<music_device> make_scc_music_device(std::span<uint8_t const> launcher,
  std::unique_ptr<midi_receiver> receiver);
std::unique_ptr<music_device> make_la_music_device(std::span<uint8_t const> launcher,
  std::unique_ptr<midi_receiver> receiver);

} // namespace tfx::audio
