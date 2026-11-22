#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include "audio/osl_fm_driver.h"
#include "audio/osl_la_driver.h"
#include "audio/osl_scc_driver.h"
#include "audio/osl_sequencer.h"

namespace {
uint64_t checksum(std::span<uint8_t const> const bytes) {
  uint64_t value{14695981039346656037ull};
  for(auto const byte : bytes) value = (value ^ byte) * 1099511628211ull;
  return value;
}
std::vector<uint8_t> channel_bytes(std::span<tfx::audio::osl_channel const> const channels) {
  std::vector<uint8_t> output;
  auto const word{[&](uint16_t const value){
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
  }};
  for(auto const &channel : channels) {
    output.push_back(channel.allocation);
    for(auto const note : channel.cell.notes) output.push_back(note);
    for(auto const byte : {channel.cell.instrument, channel.cell.effect, channel.cell.argument, channel.cell.auxiliary}) output.push_back(byte);
    word(channel.pitch_bend);
    word(channel.portamento_target);
    for(auto const byte : {channel.portamento_increasing, channel.portamento_step, channel.arpeggio_counter, channel.arpeggio_note, channel.previous_note, channel.volume, channel.sent_volume, channel.sent_instrument}) output.push_back(byte);
  }
  return output;
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc < 3 || argc > 7) throw std::runtime_error{"expected START.EXE OSL_FILE [VOLUME [TICKS [DUMP]]]"};
  uint8_t const volume{static_cast<uint8_t>(argc >= 4 ? std::stoul(argv[3]) : 75)};
  unsigned int const limit{argc >= 5 ? static_cast<unsigned int>(std::stoul(argv[4])) : 60000};
  bool dump{false};
  bool continue_zero{false};
  for(int option{5}; option < argc; ++option) {
    if(std::string_view{argv[option]} == "dump") {
      dump = true;
    } else if(std::string_view{argv[option]} == "continue-zero") {
      continue_zero = true;
    } else {
      throw std::runtime_error{"unknown sequencer probe option"};
    }
  }
  std::vector<uint8_t> events;
  auto const launcher{tfx::resources::read_file(argv[1])};
  auto source{tfx::resources::read_file(argv[2])};
  std::unique_ptr<tfx::audio::osl_device_driver> driver;
  if(source.at(7) == std::to_underlying(tfx::audio::osl_device::fm)) {
    driver = std::make_unique<tfx::audio::osl_fm_driver>(launcher, [&](tfx::audio::opl_write const command){
      events.push_back(command.address);
      events.push_back(command.value);
    });
  } else if(source.at(7) == std::to_underlying(tfx::audio::osl_device::sound_canvas)) {
    driver = std::make_unique<tfx::audio::osl_scc_driver>(launcher, [&](uint8_t const byte){events.push_back(byte);});
  } else {
    driver = std::make_unique<tfx::audio::osl_la_driver>(launcher, [&](uint8_t const byte){events.push_back(byte);});
  }
  tfx::audio::osl_sequencer sequencer;
  sequencer.register_device(*driver);
  driver->initialise();
  auto const resource{sequencer.load(std::move(source))};
  auto const playing{sequencer.play(resource, 0, 0, 0, volume)};
  auto const report{[&]{
    auto const &state{sequencer.state(playing)};
    for(unsigned int const value : std::initializer_list<unsigned int>{state.flags, state.channel_count, state.speed, state.clock_rate, state.subtick, state.order, state.loop_order, state.volume, state.row, sequencer.pattern_offset(playing), state.order_count, state.reverb[0], state.reverb[1], state.reverb[2], state.mask, sequencer.clock_rate()}) std::cout << value << ' ';
    std::cout << checksum(channel_bytes(sequencer.channel_states())) << ' ' << events.size() << ' ' << checksum(events) << '\n';
  }};
  report();
  events.clear();
  uint8_t previous_order{0};
  uint16_t previous_row{0};
  for(unsigned int tick{0}; tick != limit; ++tick) {
    sequencer.tick();
    report();
    auto const &state{sequencer.state(playing)};
    bool const finished{(!continue_zero && !sequencer.clock_rate()) || state.order < previous_order || (state.order == previous_order && state.row < previous_row) || (tick > 10 && !(state.flags & 1))};
    previous_order = state.order;
    previous_row = state.row;
    if(finished || tick + 1 == limit) {
      if(dump) {
        for(auto const byte : channel_bytes(sequencer.channel_states())) std::cerr << static_cast<unsigned int>(byte) << ' ';
        std::cerr << '\n';
        for(auto const byte : events) std::cerr << static_cast<unsigned int>(byte) << ' ';
        std::cerr << '\n';
      }
      break;
    }
    events.clear();
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
