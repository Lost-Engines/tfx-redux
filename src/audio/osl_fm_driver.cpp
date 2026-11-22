#include "osl_fm_driver.h"
#include <algorithm>
#include <bit>
#include <utility>

namespace tfx::audio {

osl_fm_driver::osl_fm_driver(std::span<uint8_t const> const launcher, opl_sink output) : output{std::move(output)} {
  resources::binary_view const source{launcher};
  unsigned int constexpr driver{0x57b0};
  if(source.le16(driver) != 2) throw std::runtime_error{"unexpected OSL FM driver data"};
  auto const copy{[&](unsigned int const offset, auto &destination){
    std::ranges::copy(source.slice(driver + offset, destination.size()), destination.begin());
  }};
  copy(0x1bc, reset_registers);
  copy(0x615, programs);
  copy(0x699, volumes);
  copy(0xe0f, modulators);
  copy(0xe25, carriers);
  for(unsigned int index{0}; index != instrument_count; ++index) copy(0x743 + index * 16, instruments[index]);
  // The native note-minus-twelve lookup uses an eight-bit index, including
  // out-of-table values. Retain the external lookup bytes without executing native instructions.
  for(unsigned int index{0}; index != note_frequencies.size(); ++index) note_frequencies[index] = source.le16(driver + 0x3bc + index * 2);
  auto const tables{source.slice(driver + 0xf3e, 0xfd6 - 0xf3e)};
  bend_tables.assign(tables.begin(), tables.end());
  for(unsigned int index{0}; index != voice_count; ++index) voices[index].keyed_frequency = source.byte(driver + 0x36c + index);
}
osl_device osl_fm_driver::device_type() const {
  return osl_device::fm;
}
void osl_fm_driver::write(unsigned int const address, uint8_t const value) {
  output({
  .address{static_cast<uint8_t>(address)},
  .value{value}
});
}
void osl_fm_driver::initialise() {
  // OPL timer-probe writes precede the complete 256-register reset.
  for(auto const value : {0x60, 0x80}) write(4, static_cast<uint8_t>(value));
  write(2, 255);
  write(4, 0x21);
  write(4, 0x60);
  write(4, 0x80);
  for(unsigned int address{0}; address != reset_registers.size(); ++address) write(address, reset_registers[address]);
  for(unsigned int index{0}; index != voices.size(); ++index) voices[index].keyed_frequency = reset_registers[0xb0 + index];
}
void osl_fm_driver::instrument(unsigned int const index, std::span<uint8_t const> const payload) {
  if(index >= instrument_count) return;
  auto const patch{resources::binary_view{payload}.slice(0, 16)};
  std::ranges::copy(patch, instruments[index].begin());
}
void osl_fm_driver::program(unsigned int const channel, uint8_t const value) {
  if(value < instrument_count) programs.at(channel) = value;
}
void osl_fm_driver::program_voice(unsigned int const index) {
  auto const &patch{instruments[programs.at(voices[index].owner)]};
  auto const modulator{modulators[index]};
  auto const carrier{carriers[index]};
  write(0x40 + carrier, static_cast<uint8_t>((patch[6] & 0xc0) | 63));
  write(0x20 + modulator, patch[0]);
  write(0x20 + carrier, patch[5]);
  write(0x40 + modulator, patch[1]);
  write(0x60 + modulator, patch[2]);
  write(0x60 + carrier, patch[7]);
  write(0x80 + modulator, patch[3]);
  write(0x80 + carrier, patch[8]);
  write(0xe0 + modulator, patch[4]);
  write(0xe0 + carrier, patch[9]);
  write(0xc0 + index, patch[10]);
}
void osl_fm_driver::update_volume(unsigned int const index) {
  auto const channel{voices[index].owner};
  auto const &patch{instruments[programs.at(channel)]};
  // Native 0x10c9 replaces the patch's attenuation with the quantised channel
  // volume, preserving only its key-scale bits. All nonzero volumes use 127.
  auto const attenuation{static_cast<uint8_t>((63u - (volumes.at(channel) >> 1)) & 63)};
  write(0x40 + carriers[index], static_cast<uint8_t>((patch[6] & 0xc0) | attenuation));
}
void osl_fm_driver::update_pitch(unsigned int const index) {
  auto &voice{voices[index]};
  uint16_t frequency{voice.frequency};
  uint8_t octave{voice.octave};
  auto bend{voice.bend};
  if(std::bit_cast<int16_t>(bend) < 0) bend = 0;
  bend = std::min<uint16_t>(bend, 0x3fff);
  if(bend != 0x2000) {
    bool const lower{bend < 0x2000};
    auto const magnitude{lower ? 0x2000u - bend : bend - 0x2000u};
    auto const phase{magnitude * 4 / 85};
    resources::binary_view const tables{bend_tables};
    auto const base{tables.le16(static_cast<uint16_t>(frequency - 0x157) / 20u * 2)};
    auto const coarse{static_cast<uint16_t>((phase >> 3) & 0xfe)};
    // Two original semitone/fraction tables, with 16-bit subtraction and
    // multiplication truncation before the final octave normalisation.
    auto const at{static_cast<uint16_t>(0x50 + base)};
    auto const other{static_cast<uint16_t>(lower ? at - coarse : at + coarse)};
    uint16_t const delta{static_cast<uint16_t>(lower ? tables.le16(at) - tables.le16(other) : tables.le16(other) - tables.le16(at))};
    uint16_t const slope{static_cast<uint16_t>(lower ? tables.le16(at) - tables.le16(at - 2) : tables.le16(at + 2) - tables.le16(at))};
    uint16_t const fraction{static_cast<uint16_t>(slope * (phase & 15))};
    auto const adjustment{static_cast<uint16_t>(delta + (fraction >> 4))};
    frequency = static_cast<uint16_t>(lower ? frequency - adjustment : frequency + adjustment);
    if(lower && frequency <= 0x157) {
      if(frequency > 0xab) {
        frequency = static_cast<uint16_t>((frequency - 0xab) * 2 + 0x157);
        --octave;
      } else if(frequency > 0x55) {
        frequency = static_cast<uint16_t>((frequency - 0x55) * 4 + 0x157);
        octave -= 2;
      } else {
        frequency = static_cast<uint16_t>(static_cast<uint16_t>(frequency - 0x2a) * 8 + 0x157);
        octave -= 3;
      }
    } else if(!lower && frequency >= 0x2ae) {
      if(frequency < 0x55c) {
        frequency = static_cast<uint16_t>(((frequency - 0x2ae) >> 1) + 0x157);
        ++octave;
      } else if(frequency < 0xab8) {
        frequency = static_cast<uint16_t>(((frequency - 0x55c) >> 2) + 0x157);
        octave += 2;
      } else {
        frequency = static_cast<uint16_t>(((frequency - 0xab8) >> 3) + 0x157);
        octave += 3;
      }
    }
  }
  write(0xa0 + index, static_cast<uint8_t>(frequency));
  voice.keyed_frequency = static_cast<uint8_t>((voice.keyed_frequency & 0x20) | ((frequency >> 8) & 3) | static_cast<uint8_t>(octave << 2));
  write(0xb0 + index, voice.keyed_frequency);
}
void osl_fm_driver::start(unsigned int const channel, uint8_t const note, uint16_t const bend) {
  auto const free{std::ranges::find_if(voices, [](auto const &voice){return voice.owner == 255;})};
  if(free == voices.end()) return;
  auto const index{static_cast<unsigned int>(free - voices.begin())};
  auto const frequency{note_frequencies[static_cast<uint8_t>(note - 12)]};
  free->owner = static_cast<uint8_t>(channel);
  free->frequency = frequency & 0x3ff;
  free->octave = static_cast<uint8_t>(frequency >> 12);
  free->bend = bend;
  program_voice(index);
  update_pitch(index);
  free->keyed_frequency |= 0x20;
  write(0xb0 + index, free->keyed_frequency);
  update_volume(index);
}
void osl_fm_driver::note(osl_channel &state, unsigned int const channel, uint8_t const value, uint8_t const) {
  if(!value) return;
  state.pitch_bend = 0x2000;
  silence(channel);
  state.previous_note = value;
  start(channel, value, state.pitch_bend);
}
void osl_fm_driver::extra_note(osl_channel const &state, unsigned int const channel, uint8_t const value) {
  start(channel, value, state.pitch_bend);
}
void osl_fm_driver::silence(unsigned int const channel) {
  for(unsigned int index{0}; index != voices.size(); ++index) {
    if(voices[index].owner == channel) {
      voices[index].owner = 255;
      voices[index].keyed_frequency &= 0xdf;
      write(0xb0 + index, voices[index].keyed_frequency);
    }
  }
}
void osl_fm_driver::volume(osl_channel &state, unsigned int const channel, uint8_t value) {
  if(state.sent_volume == value) return;
  if(value) value = 127;
  state.sent_volume = volumes.at(channel) = value;
  for(unsigned int index{0}; index != voices.size(); ++index) {
    if(voices[index].owner == channel) update_volume(index);
  }
}
void osl_fm_driver::pitch(osl_channel const &state, unsigned int const channel) {
  for(unsigned int index{0}; index != voices.size(); ++index) {
    if(voices[index].owner == channel) {
      voices[index].bend = state.pitch_bend;
      update_pitch(index);
    }
  }
}
void osl_fm_driver::pan(unsigned int, uint8_t) {}
void osl_fm_driver::modulation(unsigned int, uint8_t) {}
void osl_fm_driver::reverb(std::array<uint8_t, 3> const &) {}

} // namespace tfx::audio
