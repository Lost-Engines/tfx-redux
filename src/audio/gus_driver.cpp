#include "gus_driver.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "resources/binary.h"

namespace tfx::audio {

gus_parameters load_gus_parameters(std::span<uint8_t const> const firmware) {
  resources::binary_view const source{firmware};
  if(source.le16(0) != 0x5a4d) throw std::runtime_error{"Gravis driver is not an MZ executable"};
  unsigned int const header{source.le16(8) * 16u};
  // The reference dispatcher's MOV AX,DGROUP / MOV DS,AX identifies its data.
  if(source.byte(header + 0x438) != 0xb8 || source.le16(header + 0x43b) != 0xd88e) {
    throw std::runtime_error{"unexpected Gravis driver tables"};
  }
  unsigned int const data{header + source.le16(header + 0x439) * 16u};
  gus_parameters result{
    .frequency_base{source.le16(data + 0x2c7 + gus_driver::physical_voices * 2)},
    .volumes{},
  };
  for(unsigned int index{0}; index != result.volumes.size(); ++index) result.volumes[index] = source.le16(data + 0x32f + index * 2);
  if(!result.frequency_base) throw std::runtime_error{"Gravis driver has a zero frequency base"};
  return result;
}

gus_driver::gus_driver(gus_parameters const &parameters, gus_bus bus)
  : parameters{parameters},
    bus{std::move(bus)} {
  if(!parameters.frequency_base || !this->bus.read || !this->bus.write) throw std::invalid_argument{"Gravis driver needs a frequency base and register bus"};
}
void gus_driver::output(uint16_t const port, uint8_t const size, uint16_t const value) {
  bus.write({
    .port{port},
    .size{size},
    .value{value},
  });
}
void gus_driver::select(unsigned int const voice) {
  if(voice >= physical_voices) throw std::out_of_range{"Gravis voice exceeds its hardware channels"};
  output(voice_select, 1, static_cast<uint16_t>(voice));
}
void gus_driver::byte(gus_register const address, uint8_t const value) {
  output(register_select, 1, std::to_underlying(address));
  output(data_high, 1, value);
}
void gus_driver::word(gus_register const address, uint16_t const value) {
  output(register_select, 1, std::to_underlying(address));
  output(data_low, 2, value);
}
uint8_t gus_driver::read(gus_register const address) {
  output(register_select, 1, std::to_underlying(address));
  return static_cast<uint8_t>(bus.read(data_high, 1));
}
void gus_driver::repeated_byte(gus_register const address, uint8_t const value) {
  byte(address, value);
  // The original repeats control writes after seven IRQ-status port reads.
  // Those bus-delay reads do not alter registers; the host owns chip timing.
  output(data_high, 1, value);
}
void gus_driver::position(gus_register const address, uint32_t const bytes) {
  unsigned int constexpr low_address_bits{7};
  unsigned int constexpr high_address_mask{0x1fff};
  unsigned int constexpr fractional_shift{9};
  word(address, static_cast<uint16_t>((bytes >> low_address_bits) & high_address_mask));
  word(static_cast<gus_register>(std::to_underlying(address) + 1), static_cast<uint16_t>((bytes & ((1u << low_address_bits) - 1)) << fractional_shift));
}
uint16_t gus_driver::frequency_control(uint32_t const rate) const {
  unsigned int constexpr frequency_shift{9};
  // Native SHL/ADD wrap in dwords before unsigned DIV, then SHL wraps BX.
  uint32_t const numerator{static_cast<uint32_t>((rate << frequency_shift) + parameters.frequency_base / 2u)};
  return static_cast<uint16_t>(static_cast<uint16_t>(numerator / parameters.frequency_base) << 1);
}
void gus_driver::volume(unsigned int const voice, unsigned int const value) {
  select(voice);
  word(gus_register::volume, static_cast<uint16_t>(parameters.volumes.at(value) << 4));
}
bool gus_driver::stopped(unsigned int const voice) {
  select(voice);
  return (read(gus_register::voice_status) & wave_stopped_mask) != 0;
}
void gus_driver::play(unsigned int const voice, uint32_t const begin, uint32_t const end, uint32_t const rate) {
  select(voice);
  word(gus_register::frequency, frequency_control(rate));
  select(voice);
  repeated_byte(gus_register::ramp_control, static_cast<uint8_t>(read(gus_register::ramp_status) & ~ramp_pcm_bit));
  position(gus_register::current_high, begin);
  position(gus_register::loop_begin_high, std::min(begin, end));
  position(gus_register::loop_end_high, std::max(begin, end));
  select(voice);
  repeated_byte(gus_register::voice_control, static_cast<uint8_t>(wave_irq_bit | (begin > end ? wave_reverse_bit : 0)));
}
void gus_driver::stop(unsigned int const voice) {
  select(voice);
  repeated_byte(gus_register::ramp_control, static_cast<uint8_t>(read(gus_register::ramp_status) & ~ramp_pcm_bit));
  repeated_byte(gus_register::voice_control, static_cast<uint8_t>((read(gus_register::voice_status) & ~wave_irq_bit) | wave_stopped_mask));
}

} // namespace tfx::audio
