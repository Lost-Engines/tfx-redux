#pragma once

#include <functional>
#include "audio/osl_device.h"

namespace tfx::audio {

struct opl_write {
  uint8_t address;
  uint8_t value;
  bool operator==(opl_write const &) const = default;
};
using opl_sink = std::function<void(opl_write)>;

class osl_fm_driver final : public osl_device_driver {
private:
  unsigned int static constexpr voice_count{9};
  unsigned int static constexpr instrument_count{100};
  struct voice {
    uint8_t owner{255};
    uint16_t frequency{0};
    uint8_t octave{0};
    uint16_t bend{0x2000};
    uint8_t keyed_frequency{0};
  };
  std::array<voice, voice_count> voices;
  std::array<uint8_t, 16> programs{};
  std::array<uint8_t, 16> volumes;
  std::array<std::array<uint8_t, 16>, instrument_count> instruments;
  std::array<uint8_t, 256> reset_registers;
  std::array<uint16_t, 256> note_frequencies;
  resources::bytes bend_tables;
  std::array<uint8_t, voice_count> modulators;
  std::array<uint8_t, voice_count> carriers;
  opl_sink output;
  void write(unsigned int address, uint8_t value);
  void start(unsigned int channel, uint8_t note, uint16_t bend);
  void update_pitch(unsigned int index);
  void update_volume(unsigned int index);
  void program_voice(unsigned int index);

public:
  osl_fm_driver(std::span<uint8_t const> launcher, opl_sink output);
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
