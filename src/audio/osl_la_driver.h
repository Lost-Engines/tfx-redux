#pragma once

#include "audio/midi_byte_sink.h"
#include "audio/osl_device.h"

namespace tfx::audio {

class osl_la_driver final : public osl_device_driver {
private:
  resources::bytes native_data;
  midi_byte_sink output;
  uint8_t last_status;
  void byte(uint8_t value);
  void message(uint8_t status, uint8_t first, uint8_t second);
  void sysex(unsigned int offset);
  void bend(unsigned int channel, uint16_t value);

public:
  // Only original parameter templates/initial cache data are read. No native
  // instructions are interpreted or executed by this translated device driver.
  osl_la_driver(std::span<uint8_t const> launcher, midi_byte_sink output);
  osl_device device_type() const override;
  void initialise() override;
  void instrument(unsigned int index, std::span<uint8_t const> payload) override;
  void note(osl_channel &state, unsigned int channel, uint8_t note, uint8_t velocity) override;
  void extra_note(osl_channel const &state, unsigned int channel, uint8_t note) override;
  void silence(unsigned int channel) override;
  void program(unsigned int channel, uint8_t value) override;
  void pan(unsigned int channel, uint8_t value) override;
  void volume(osl_channel &state, unsigned int channel, uint8_t value) override;
  void pitch(osl_channel const &state, unsigned int channel) override;
  void modulation(unsigned int channel, uint8_t value) override;
  void reverb(std::array<uint8_t, 3> const &values) override;
};

} // namespace tfx::audio
