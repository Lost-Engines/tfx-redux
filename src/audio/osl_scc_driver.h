#pragma once

#include "audio/midi_byte_sink.h"
#include "audio/osl_device.h"

namespace tfx::audio {

class osl_scc_driver final : public osl_device_driver {
private:
  struct patch {
    uint8_t bank;
    uint8_t program;
    uint8_t rhythm;
  };
  std::array<patch, 100> patches;
  resources::bytes parameters;
  midi_byte_sink output;
  void byte(uint8_t value);
  void message(uint8_t status, uint8_t first, uint8_t second);
  void sysex(unsigned int position);
  void bend(unsigned int channel, uint16_t value);
  void reset_controls();
  void effect_parameter(uint8_t address, uint8_t value);
  void reset_effects(unsigned int channel);
  void nrpn(unsigned int channel, uint16_t parameter, uint8_t value);
  void reset_nrpn(unsigned int channel);

public:
  osl_scc_driver(std::span<uint8_t const> launcher, midi_byte_sink output);
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
  void extension(osl_channel const &state, unsigned int channel) override;
};

} // namespace tfx::audio
