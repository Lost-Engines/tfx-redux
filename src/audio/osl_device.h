#pragma once

#include "audio/osl_channel.h"

namespace tfx::audio {

// The sequencer owns channels and timing; a device owns its hardware voices
// and patch/register state. This is the original OSL driver's callback boundary.
class osl_device_driver {
public:
  virtual ~osl_device_driver() = default;
  virtual osl_device device_type() const = 0;
  virtual void initialise() = 0;
  virtual void instrument(unsigned int index, std::span<uint8_t const> payload) = 0;
  virtual void note(osl_channel &state, unsigned int channel, uint8_t value, uint8_t velocity) = 0;
  virtual void extra_note(osl_channel const &state, unsigned int channel, uint8_t value) = 0;
  virtual void silence(unsigned int channel) = 0;
  virtual void program(unsigned int channel, uint8_t value) = 0;
  virtual void pan(unsigned int channel, uint8_t value) = 0;
  virtual void volume(osl_channel &state, unsigned int channel, uint8_t value) = 0;
  virtual void pitch(osl_channel const &state, unsigned int channel) = 0;
  virtual void modulation(unsigned int channel, uint8_t value) = 0;
  virtual void reverb(std::array<uint8_t, 3> const &values) = 0;
  // LA and FM return unchanged; SCC implements GS/NRPN extension commands.
  virtual void extension(osl_channel const &, unsigned int) {}
};

} // namespace tfx::audio
