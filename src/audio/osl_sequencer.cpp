#include "osl_sequencer.h"
#include <algorithm>
#include <bit>
#include <utility>

namespace tfx::audio {

osl_sequencer::osl_sequencer() {
  // Unallocated native channels are zero-filled. Allocation assigns the two
  // 255 output-cache sentinels and the initial 255 instrument cell separately.
  for(auto &channel : channels) channel = {
    .sent_volume{0},
    .sent_instrument{0}
  };
}
void osl_sequencer::register_device(osl_device_driver &driver) {
  if(device(driver.device_type())) throw std::invalid_argument{"OSL device is already registered"};
  devices.push_back(&driver);
}
osl_device_driver *osl_sequencer::device(osl_device const type) const {
  auto const found{std::ranges::find_if(devices,[&](auto const *driver){ return driver->device_type() == type; })};
  return found == devices.end() ? nullptr : *found;
}
osl_song osl_sequencer::song(playing const &voice) const {
  return voice.source->song(voice.state.song);
}
osl_resource_id osl_sequencer::load(resources::bytes source) {
  auto const found{std::ranges::find_if(resources,[](auto const &value){ return !value; })};
  if(found == resources.end()) throw std::runtime_error{"OSL has no free resource slots"};
  *found = std::make_shared<osl_resource>(std::move(source));
  auto const &resource{**found};
  for(unsigned int index{0}; index != resource.instrument_count(); ++index) {
    auto const instrument{resource.instrument(index)};
    if(instrument.empty()) continue;
    resources::binary_view const common{instrument};
    if(auto *const driver{device(static_cast<osl_device>(common.byte(0x20)))}) driver->instrument(index,instrument.subspan(0x2a));
  }
  return static_cast<unsigned int>(found - resources.begin());
}
void osl_sequencer::release(osl_resource_id const resource) {
  // Command 3 releases the handle without stopping its active playback.
  resources.at(resource).reset();
}
osl_playing_id osl_sequencer::play(osl_resource_id const resource, unsigned int const index, uint8_t const order, uint16_t const row, uint8_t const volume) {
  auto const result{try_play(resource,index,order,row,volume)};
  if(!result) throw std::runtime_error{"OSL has insufficient free playback slots or channels"};
  return *result;
}
std::optional<osl_playing_id> osl_sequencer::try_play(osl_resource_id const resource, unsigned int const index, uint8_t const order, uint16_t const row, uint8_t const volume) {
  if(!resources.at(resource)) throw std::out_of_range{"OSL resource is not loaded"};
  auto const data{resources[resource]->song(index)};
  auto const found{std::ranges::find_if(songs,[](auto const &voice){ return !(voice.state.flags & 1); })};
  if(found == songs.end()) return {};
  if(data.channels() > osl_channels || std::ranges::count_if(channels,[](auto const &channel){ return !channel.allocation; }) < data.channels()) return {};
  *found = {};
  found->source = resources[resource];
  auto &voice{found->state};
  voice.resource = resource;
  voice.song = index;
  voice.channel_count = static_cast<uint8_t>(data.channels());
  voice.speed = data.speed();
  voice.clock_rate = clock = data.clock_rate();
  auto const initial{static_cast<uint8_t>(voice.speed - 1)};
  voice.subtick = std::bit_cast<int8_t>(initial) < 0 ? 0 : initial;
  voice.order = order;
  voice.loop_order = static_cast<uint8_t>(data.loop_order());
  voice.order_count = static_cast<uint8_t>(data.order_count());
  voice.volume = volume;
  unsigned int next{0};
  for(unsigned int channel{0}; channel != voice.channel_count; ++channel) {
    while(channels[next].allocation) ++next;
    voice.channel_slots[channel] = next;
    channels[next] = {};
    channels[next].cell.instrument = 255;
    channels[next].allocation = static_cast<uint8_t>(next + 1);
    ++next;
  }
  // Native 0xbe9 decodes skipped rows without running their note/effect events.
  for(unsigned int skipped{0}; skipped != row; ++skipped) read_row(*found);
  voice.flags = 1;
  return static_cast<unsigned int>(found - songs.begin());
}
void osl_sequencer::stop(osl_playing_id const id) {
  auto &voice{songs.at(id).state};
  if(!(voice.flags & 1)) return;
  voice.flags &= 254;
  auto *const driver{device(songs.at(id).source->device())};
  for(unsigned int logical{0}; logical != voice.channel_count; ++logical) {
    auto &channel{channels[voice.channel_slots[logical]]};
    if(driver) driver->silence(channel.allocation - 1u);
    channel.allocation = 0;
  }
}
void osl_sequencer::stop_all() {
  for(unsigned int index{0}; index != songs.size(); ++index) stop(index);
}
void osl_sequencer::volume(osl_playing_id const id, uint8_t const value) {
  auto &voice{songs.at(id)};
  if(!(voice.state.flags & 1)) return;
  voice.state.volume = value;
  auto *const driver{device(voice.source->device())};
  if(driver) for(unsigned int logical{0}; logical != voice.state.channel_count; ++logical) {
    auto const physical{voice.state.channel_slots[logical]};
    send_volume(voice,channels[physical],physical,*driver);
  }
}
uint8_t osl_sequencer::transpose(playing const &voice, osl_channel const &channel) const {
  auto const &resource{*voice.source};
  uint8_t value{song(voice).transpose()};
  auto const instrument{resource.instrument_parameters(channel.sent_instrument)};
  if(!instrument.empty()) value = static_cast<uint8_t>(value + resources::binary_view{instrument}.byte(0x18));
  return value;
}
void osl_sequencer::read_row(playing &voice) {
  if(!voice.pattern) {
    auto const data{song(voice)};
    auto const pattern{data.pattern(data.order(voice.state.order))};
    voice.pattern_rows = pattern.rows;
    voice.pattern.emplace(pattern);
  }
  auto const row{voice.pattern->next(voice.state.channel_count)};
  ++voice.state.row;
  for(unsigned int logical{0}; logical != voice.state.channel_count; ++logical) channels[voice.state.channel_slots[logical]].cell = row[logical];
}
void osl_sequencer::reset_pattern(playing &voice) {
  voice.state.subtick = 0;
  voice.state.row = 0;
  voice.pattern.reset();
  if(!(voice.state.flags & 2)) {
    ++voice.state.order;
    if(voice.state.order == voice.state.order_count) voice.state.order = voice.state.loop_order;
  }
}
void osl_sequencer::send_volume(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver) {
  auto value{channel.volume};
  if(physical >= 16 || !(voice.state.mask & (1u << physical))) value = 0;
  uint8_t cap{0};
  auto const &resource{*voice.source};
  auto const instrument{resource.instrument_parameters(channel.sent_instrument)};
  if(!instrument.empty()) cap = resources::binary_view{instrument}.byte(0x19);
  value = std::min(value,cap);
  auto const data{song(voice)};
  auto const channel_volume{resources::binary_view{data.resource}.byte(data.offset + 0x16 + physical)};
  value = static_cast<uint8_t>((value * channel_volume) >> 7);
  value = static_cast<uint8_t>((value * voice.state.volume) >> 7);
  driver.volume(channel,physical,value);
}
void osl_sequencer::arpeggio(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver) {
  uint8_t flags{16};
  if(!channel.cell.notes[0]) {
    --channel.arpeggio_counter;
    if(std::bit_cast<int8_t>(channel.arpeggio_counter) >= 0) return;
    flags = channel.cell.argument;
  }
  channel.arpeggio_counter = channel.cell.argument & 15;
  for(auto &note : channel.cell.notes) if(note) {
    auto const value{static_cast<uint8_t>(note + transpose(voice,channel))};
    note = 0;
    if(flags & 16) driver.note(channel,physical,value,127);
    else driver.extra_note(channel,physical,value);
    break;
  }
}
void osl_sequencer::retrigger(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver, bool const new_row) {
  if(new_row && channel.cell.notes[0]) {
    channel.arpeggio_note = channel.cell.notes[0];
    channel.arpeggio_counter = static_cast<uint8_t>((channel.cell.argument >> 4) | (channel.cell.argument << 4));
    channel.cell.notes[0] = 0;
  }
  if((channel.arpeggio_counter & 15) > 1) { --channel.arpeggio_counter;return; }
  channel.arpeggio_counter = static_cast<uint8_t>(channel.arpeggio_counter | (channel.arpeggio_counter >> 4));
  if(channel.arpeggio_note) driver.note(channel,physical,static_cast<uint8_t>(channel.arpeggio_note + transpose(voice,channel)),127);
}
void osl_sequencer::row_effect(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver, bool &jump, bool &stop) {
  if(channel.cell.effect & 128) { driver.extension(channel,physical);return; }
  auto const argument{channel.cell.argument};
  switch(static_cast<osl_effect>(channel.cell.effect & 31)) {
  // Native MEDIT scales the nibble before the device callback. LA applies
  // its own nibble scaling too; retain both stages rather than correcting it.
  case osl_effect::modulation: driver.modulation(physical,static_cast<uint8_t>((argument & 15) * 8));break;
  case osl_effect::pan: {
    uint8_t value{64};
    if(argument & 240) value = static_cast<uint8_t>(64 + ((argument & 240) >> 2));
    else if(argument & 15) value = static_cast<uint8_t>(64 - (argument & 15) * 4);
    else channel.cell.effect = 0;
    driver.pan(physical,value);
    break;
  }
  case osl_effect::silence: channel.cell.effect = 0;driver.silence(physical);break;
  case osl_effect::reverb_mode: channel.cell.effect = 0;voice.state.reverb[0] = argument;driver.reverb(voice.state.reverb);break;
  case osl_effect::reverb_parameters:
    channel.cell.effect = 0;
    voice.state.reverb[1] = argument & 15;
    voice.state.reverb[2] = argument >> 4;
    driver.reverb(voice.state.reverb);
    break;
  case osl_effect::speed: if(argument & 31) { voice.state.subtick = 0;voice.state.speed = argument & 31; }break;
  case osl_effect::order_jump: voice.state.order = static_cast<uint8_t>(argument - 1);jump = true;break;
  case osl_effect::volume: channel.volume = argument;send_volume(voice,channel,physical,driver);break;
  case osl_effect::stop: stop = true;break;
  case osl_effect::clock_rate: voice.state.clock_rate = clock = argument;break;
  case osl_effect::arpeggio: arpeggio(voice,channel,physical,driver);break;
  case osl_effect::retrigger: retrigger(voice,channel,physical,driver,true);break;
  default: break;
  }
}
void osl_sequencer::tick_effect(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver) {
  if(channel.cell.effect & 128) { driver.extension(channel,physical);return; }
  auto const argument{channel.cell.argument};
  switch(static_cast<osl_effect>(channel.cell.effect & 31)) {
  case osl_effect::pitch_up: channel.pitch_bend = static_cast<uint16_t>(channel.pitch_bend + argument * 20);driver.pitch(channel,physical);break;
  case osl_effect::pitch_down: channel.pitch_bend = static_cast<uint16_t>(channel.pitch_bend - argument * 20);driver.pitch(channel,physical);break;
  case osl_effect::portamento:
    if(argument) { channel.portamento_step = argument;channel.cell.argument = 0; }
    if(channel.portamento_target) {
      auto const step{channel.portamento_step * 20};
      if(channel.portamento_increasing) {
        channel.pitch_bend = static_cast<uint16_t>(channel.pitch_bend + step);
        if(channel.pitch_bend >= channel.portamento_target) { channel.pitch_bend = channel.portamento_target;channel.portamento_target = 0; }
      } else {
        channel.pitch_bend = static_cast<uint16_t>(channel.pitch_bend - step);
        if(channel.pitch_bend <= channel.portamento_target) { channel.pitch_bend = channel.portamento_target;channel.portamento_target = 0; }
      }
      driver.pitch(channel,physical);
    }
    break;
  case osl_effect::volume_slide:
    if(argument >> 4) {
      channel.volume = static_cast<uint8_t>(channel.volume + (argument >> 4) * 2);
      if(channel.volume > 127) channel.volume = 127;
    } else {
      unsigned int const decrement{(argument & 15) * 2u};
      channel.volume = channel.volume < decrement ? 0 : static_cast<uint8_t>(channel.volume - decrement);
    }
    send_volume(voice,channel,physical,driver);
    break;
  case osl_effect::arpeggio: arpeggio(voice,channel,physical,driver);break;
  case osl_effect::retrigger: retrigger(voice,channel,physical,driver,false);break;
  default: break;
  }
}
void osl_sequencer::new_notes(playing &voice, osl_channel &channel, unsigned int const physical, osl_device_driver &driver, bool &jump, bool &stop) {
  bool const notes{std::ranges::any_of(channel.cell.notes,[](auto const value){ return value != 0; })};
  if(channel.cell.instrument && notes) {
    uint8_t const instrument{static_cast<uint8_t>(channel.cell.instrument - 1)};
    if(instrument != channel.sent_instrument) { channel.sent_instrument = instrument;driver.program(physical,instrument); }
    channel.volume = 127;
    if(channel.cell.effect == std::to_underlying(osl_effect::volume)) { channel.volume = channel.cell.argument;channel.cell.effect = channel.cell.argument = 0; }
    send_volume(voice,channel,physical,driver);
  }
  if(notes && channel.cell.effect == std::to_underlying(osl_effect::portamento)) {
    // Native 0x1478 sign-extends the wrapped semitone byte (CBW).
    auto const difference{std::bit_cast<int8_t>(static_cast<uint8_t>(channel.cell.notes[0] + transpose(voice,channel) - channel.previous_note))};
    channel.portamento_target = static_cast<uint16_t>((difference * 341 + 8192) & 16383);
    channel.portamento_increasing = channel.portamento_target > channel.pitch_bend;
    if(channel.portamento_target == channel.pitch_bend) channel.portamento_target = 0;
    row_effect(voice,channel,physical,driver,jump,stop);
    return;
  }
  row_effect(voice,channel,physical,driver,jump,stop);
  if(!notes || channel.cell.effect == std::to_underlying(osl_effect::arpeggio)) return;
  for(unsigned int index{0}; index != channel.cell.notes.size(); ++index) if(channel.cell.notes[index]) {
    auto const note{static_cast<uint8_t>(channel.cell.notes[index] + transpose(voice,channel))};
    if(!index) driver.note(channel,physical,note,127);
    else driver.extra_note(channel,physical,note);
  }
}
void osl_sequencer::tick() {
  for(unsigned int id{0}; id != songs.size(); ++id) {
    auto &voice{songs[id]};
    if(!(voice.state.flags & 1)) continue;
    ++voice.state.subtick;
    bool const row{voice.state.subtick == voice.state.speed};
    bool jump{false}, end{false};
    if(row) { voice.state.subtick = 0;read_row(voice); }
    auto *const driver{device(voice.source->device())};
    if(driver) for(unsigned int logical{0}; logical != voice.state.channel_count; ++logical) {
      auto const physical{voice.state.channel_slots[logical]};
      auto &channel{channels[physical]};
      if(row) new_notes(voice,channel,physical,*driver,jump,end);
      else tick_effect(voice,channel,physical,*driver);
      send_volume(voice,channel,physical,*driver);
    }
    if(row && (jump || voice.state.row >= voice.pattern_rows)) reset_pattern(voice);
    if(end) stop(id);
  }
}
uint16_t osl_sequencer::clock_rate() const {
  return clock;
}
unsigned int osl_sequencer::pattern_offset(osl_playing_id const id) const {
  return songs.at(id).pattern ? songs[id].pattern->offset() : 0;
}
osl_playing_state const &osl_sequencer::state(osl_playing_id const id) const {
  return songs.at(id).state;
}
std::span<osl_channel const> osl_sequencer::channel_states() const {
  return channels;
}

} // namespace tfx::audio
