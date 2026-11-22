#include "osl_scc_driver.h"
#include <algorithm>
#include <utility>

namespace tfx::audio {

osl_scc_driver::osl_scc_driver(std::span<uint8_t const> const launcher, midi_byte_sink output) : parameters(0x7ba), output{std::move(output)} {
  resources::binary_view const source{launcher};
  unsigned int constexpr driver{0x6890};
  if(source.le16(driver) != 8) throw std::runtime_error{"unexpected OSL SCC driver data"};
  for(auto const &[start,end] : {std::pair{0x182u,0x226u},std::pair{0x7b0u,0x7bau}}) {
    auto const bytes{source.slice(driver + start,end - start)};
    std::ranges::copy(bytes,parameters.begin() + start);
  }
  for(unsigned int index{0}; index != patches.size(); ++index) patches[index] = {
    .bank{source.byte(driver + 0x3fe + index * 4)},
    .program{source.byte(driver + 0x3ff + index * 4)},
    .rhythm{source.byte(driver + 0x400 + index * 4)}
  };
}
osl_device osl_scc_driver::device_type() const {
  return osl_device::sound_canvas;
}
void osl_scc_driver::byte(uint8_t const value) {
  output(value);
}
void osl_scc_driver::message(uint8_t const status, uint8_t const first, uint8_t const second) {
  byte(status);
  byte(first);
  byte(second);
}
void osl_scc_driver::sysex(unsigned int position) {
  for(unsigned int index{0}; index != 5; ++index) byte(parameters.at(position++));
  unsigned int count{parameters.at(position++) + 3u};
  uint8_t sum{0};
  while(count--) { auto const value{parameters.at(position++)};sum = static_cast<uint8_t>(sum + value);byte(value); }
  byte(static_cast<uint8_t>((0u - sum) & 127));byte(0xf7);
}
void osl_scc_driver::reset_controls() {
  for(unsigned int channel{0}; channel != 16; ++channel) {
    auto const status{static_cast<uint8_t>(0xb0 + channel)};
    message(status,121,0);message(status,10,64);
    message(status,100,0);message(status,101,0);message(status,6,24);
  }
  sysex(0x1ce);sysex(0x1de);
}
void osl_scc_driver::initialise() {
  sysex(0x182);sysex(0x18c);sysex(0x198);
  for(unsigned int channel{0}; channel != 16; ++channel) {
    parameters[0x1ba] = static_cast<uint8_t>(channel);
    parameters[0x1b8] = static_cast<uint8_t>(channel | 16);
    sysex(0x1b1);
  }
  reset_controls();
}
void osl_scc_driver::instrument(unsigned int const index, std::span<uint8_t const> const payload) {
  if(index >= patches.size()) return;
  resources::binary_view const bytes{payload};
  patches[index] = {
    .bank{bytes.byte(1)},
    .program{bytes.byte(2)},
    .rhythm{bytes.byte(0)}
  };
}
void osl_scc_driver::program(unsigned int const channel, uint8_t const value) {
  auto const &patch{patches.at(value)};
  if(patch.rhythm == 1) { parameters[0x219] = static_cast<uint8_t>(channel | 16);sysex(0x212); } else {
    parameters[0x223] = static_cast<uint8_t>(channel | 16);sysex(0x21c);
    message(static_cast<uint8_t>(0xb0 + channel),0,patch.bank);
  }
  byte(static_cast<uint8_t>(0xc0 + channel));byte(patch.program);
}
void osl_scc_driver::bend(unsigned int const channel, uint16_t const value) {
  message(static_cast<uint8_t>(0xe0 + channel),value & 127,(value >> 7) & 127);
}
void osl_scc_driver::note(osl_channel &state, unsigned int const channel, uint8_t const value, uint8_t const velocity) {
  if(!value) return;
  if(state.pitch_bend != 0x2000) { state.pitch_bend = 0x2000;bend(channel,state.pitch_bend); }
  silence(channel);state.previous_note = value;message(static_cast<uint8_t>(0x90 + channel),value,velocity);
}
void osl_scc_driver::extra_note(osl_channel const &, unsigned int const channel, uint8_t const value) {
  if(value) message(static_cast<uint8_t>(0x90 + channel),value,127);
}
void osl_scc_driver::silence(unsigned int const channel) {
  message(static_cast<uint8_t>(0xb0 + channel),123,0);
}
void osl_scc_driver::pan(unsigned int const channel, uint8_t const value) {
  message(static_cast<uint8_t>(0xb0 + channel),10,value);
}
void osl_scc_driver::volume(osl_channel &state, unsigned int const channel, uint8_t const value) {
  if(state.sent_volume == value) return;
  state.sent_volume = value;message(static_cast<uint8_t>(0xb0 + channel),7,value);
}
void osl_scc_driver::pitch(osl_channel const &state, unsigned int const channel) {
  bend(channel,state.pitch_bend);
}
void osl_scc_driver::modulation(unsigned int const channel, uint8_t const value) {
  message(static_cast<uint8_t>(0xb0 + channel),1,static_cast<uint8_t>((value & 15) * 8));
}
void osl_scc_driver::reverb(std::array<uint8_t,3> const &) {}
void osl_scc_driver::effect_parameter(uint8_t const address, uint8_t const value) {
  parameters[0x7b8] = address;
  parameters[0x7b9] = value;
  sysex(0x7b0);
}
void osl_scc_driver::reset_effects(unsigned int const channel) {
  message(static_cast<uint8_t>(0xb0 | channel),91,0);message(static_cast<uint8_t>(0xb0 | channel),93,0);
  std::array<uint8_t,7> constexpr values{7,2,5,127,127,127,0};
  for(unsigned int index{0}; index != values.size(); ++index) effect_parameter(static_cast<uint8_t>(0x30 + index),values[index]);
}
void osl_scc_driver::nrpn(unsigned int const channel, uint16_t const parameter, uint8_t const value) {
  auto const status{static_cast<uint8_t>(0xb0 | channel)};
  message(status,99,static_cast<uint8_t>(parameter >> 8));message(status,98,static_cast<uint8_t>(parameter));
  message(status,6,value);message(status,38,17);
}
void osl_scc_driver::reset_nrpn(unsigned int const channel) {
  for(auto const value : {0x108,0x109,0x10a,0x120,0x121,0x163,0x164,0x166}) nrpn(channel,static_cast<uint16_t>(value),64);
}
void osl_scc_driver::extension(osl_channel const &state, unsigned int const channel) {
  auto const command{state.cell.effect & 127};
  auto const argument{state.cell.argument};
  if(command == 0 || command == 1) message(static_cast<uint8_t>(0xb0 + channel),command ? 93 : 91,argument);
  else if(command <= 8) effect_parameter(static_cast<uint8_t>(0x30 + command - 2),argument & (command <= 4 ? 7 : 127));
  else if(command <= 16) {
    // The last native chorus command reuses address 0x38, rather than 0x3f.
    auto const address{static_cast<uint8_t>(command == 16 ? 0x38 : 0x38 + command - 9)};
    effect_parameter(address,argument & (command <= 10 ? 7 : 127));
  } else if(command <= 24) {
    std::array<uint16_t,8> constexpr parameters{0x108,0x109,0x10a,0x120,0x121,0x163,0x164,0x166};
    nrpn(channel,parameters[static_cast<unsigned int>(command - 17)],argument);
  } else if(command == 25) for(unsigned int part{0}; part != 8; ++part) reset_effects(part);
  else if(command == 26) reset_effects(channel);
  else if(command == 27) reset_nrpn(channel);
  else if(command == 28) for(unsigned int part{0}; part != 8; ++part) reset_nrpn(part);
  else if(command == 29) {
    // This unused native command calls the device reset with DS still pointing
    // at MEDIT, then reads its SysEx templates from shared playback state.
    // No stored SCC pattern uses it; do not invent portable garbage bytes.
    throw std::runtime_error{"OSL SCC reset command 29 requires untranslated native data-segment context"};
  }
}

} // namespace tfx::audio
