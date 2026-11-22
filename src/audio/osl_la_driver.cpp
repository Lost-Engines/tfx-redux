#include "osl_la_driver.h"
#include <algorithm>
#include <utility>

namespace tfx::audio {

osl_la_driver::osl_la_driver(std::span<uint8_t const> const launcher, midi_byte_sink output)
  :
    native_data(0x308),
    output{std::move(output)},
    last_status{0} {
  resources::binary_view const source{launcher};
  unsigned int constexpr driver{0x7120};
  if(source.le16(driver) != 4) throw std::runtime_error{"unexpected OSL LA driver data"};
  // Fixed reset, patch/timbre, system and reverb parameter templates only.
  auto const parameters{source.slice(driver + 0x20f, 0x308 - 0x20f)};
  std::ranges::copy(parameters, native_data.begin() + 0x20f);
  last_status = source.byte(driver + 0x1f0);
}
osl_device osl_la_driver::device_type() const {
  return osl_device::lapc;
}
void osl_la_driver::byte(uint8_t const value) {
  if(value & 0x80) {
    if(value == last_status) return;
    last_status = value;
  }
  output(value);
}
void osl_la_driver::message(uint8_t const status, uint8_t const first, uint8_t const second) {
  byte(status);
  byte(first);
  byte(second);
}
void osl_la_driver::sysex(unsigned int position) {
  unsigned int count{native_data.at(position++)};
  while(count--) byte(native_data.at(position++));
  count = native_data.at(position++);
  uint8_t sum{0};
  while(count--) {
    auto const value{native_data.at(position++)};
    sum = static_cast<uint8_t>(sum + value);
    byte(value);
  }
  byte(static_cast<uint8_t>((0u - sum) & 127));
  byte(0xf7);
}
void osl_la_driver::initialise() {
  sysex(0x2bd);
  for(unsigned int channel{1}; channel != 10; ++channel) {
    message(static_cast<uint8_t>(0xb0 + channel), 123, 0);
    message(static_cast<uint8_t>(0xb0 + channel), 64, 0);
    bend(channel, 0x2000);
  }
  sysex(0x20f);
}
void osl_la_driver::instrument(unsigned int const index, std::span<uint8_t const> const payload) {
  resources::binary_view const source{payload};
  source.slice(0, 244);
  auto const patch{source.slice(0, 8)};
  std::ranges::copy(patch, native_data.begin() + 0x28a);
  auto const address{static_cast<uint16_t>(static_cast<uint8_t>(index) * 8u)};
  native_data[0x288] = static_cast<uint8_t>(address / 128);
  native_data[0x289] = static_cast<uint8_t>(address % 128);
  if(native_data[0x28a] != 3) native_data[0x28b] = static_cast<uint8_t>(index);
  native_data[0x28e] = 24;
  sysex(0x280);
  uint8_t low{10};
  uint8_t high{static_cast<uint8_t>(index * 2u)};
  native_data[0x27a] = high;
  native_data[0x27b] = low;
  auto const common{source.slice(8, 4)};
  std::ranges::copy(common, native_data.begin() + 0x27c);
  sysex(0x272);
  low += 4;
  for(unsigned int partial{0}; partial != 4; ++partial) {
    native_data[0x236] = high;
    native_data[0x237] = low;
    auto const parameters{source.slice(12 + partial * 58, 58)};
    std::ranges::copy(parameters, native_data.begin() + 0x238);
    sysex(0x22e);
    low = static_cast<uint8_t>(low + 58);
    if(low > 127) {
      low &= 127;
      ++high;
    }
  }
}
void osl_la_driver::bend(unsigned int const channel, uint16_t const value) {
  message(static_cast<uint8_t>(0xe0 + channel), value & 127, (value >> 7) & 127);
}
void osl_la_driver::note(osl_channel &state, unsigned int const channel, uint8_t const value, uint8_t const velocity) {
  if(!value) return;
  if(state.pitch_bend != 0x2000) {
    state.pitch_bend = 0x2000;
    bend(channel, state.pitch_bend);
  }
  silence(channel);
  byte(static_cast<uint8_t>(0x90 + channel));
  state.previous_note = value;
  byte(value);
  byte(velocity);
}
void osl_la_driver::extra_note(osl_channel const &, unsigned int const channel, uint8_t const value) {
  if(value) message(static_cast<uint8_t>(0x90 + channel), value, 127);
}
void osl_la_driver::silence(unsigned int const channel) {
  message(static_cast<uint8_t>(0xb0 + channel), 123, 0);
}
void osl_la_driver::program(unsigned int const channel, uint8_t const value) {
  byte(static_cast<uint8_t>(0xc0 + channel));
  byte(value);
}
void osl_la_driver::pan(unsigned int const channel, uint8_t const value) {
  message(static_cast<uint8_t>(0xb0 + channel), 10, value);
}
void osl_la_driver::volume(osl_channel &state, unsigned int const channel, uint8_t const value) {
  if(state.sent_volume == value) return;
  state.sent_volume = value;
  message(static_cast<uint8_t>(0xb0 + channel), 7, value);
}
void osl_la_driver::pitch(osl_channel const &state, unsigned int const channel) {
  bend(channel, state.pitch_bend);
}
void osl_la_driver::modulation(unsigned int const channel, uint8_t const value) {
  message(static_cast<uint8_t>(0xb0 + channel), 1, static_cast<uint8_t>((value & 15) * 8));
}
void osl_la_driver::reverb(std::array<uint8_t, 3> const &values) {
  std::ranges::copy(values, native_data.begin() + 0x29c);
  sysex(0x292);
}

} // namespace tfx::audio
