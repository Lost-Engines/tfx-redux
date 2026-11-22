#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "audio/midi_stream.h"

struct receiver final : tfx::audio::midi_receiver {
  std::vector<uint32_t> messages;
  std::vector<std::vector<uint8_t>> system;
  void short_message(uint32_t value) override {
    messages.push_back(value);
  }
  void sysex(std::span<uint8_t const> value) override {
    system.emplace_back(value.begin(), value.end());
  }
  void render(std::span<float>) override {}
};
int main() try {
  receiver output;
  tfx::audio::midi_stream input{output};
  auto const send{[&](std::initializer_list<uint8_t> const bytes){
    for(auto const byte : bytes) input.byte(byte);
  }};
  send({10, 20, 0x90, 60, 0xf8, 100, 61, 110, 0xc1, 7, 8});
  if(output.messages != std::vector<uint32_t>{0xf8, 0x643c90, 0x6e3d90, 0x7c1, 0x8c1}) throw std::runtime_error{"channel/running-status or interleaved real-time message changed"};
  send({0xf0, 0x41, 0x10, 0x90, 62, 127});
  if(!output.system.empty() || output.messages.back() != 0x7f3e90) throw std::runtime_error{"interrupted SysEx was not discarded"};
  send({0xf0, 0x41, 0x10, 0x16, 0x12, 0, 0, 0, 127, 0xf7, 1, 2});
  if(output.system != std::vector<std::vector<uint8_t>>{{0xf0, 0x41, 0x10, 0x16, 0x12, 0, 0, 0, 127, 0xf7}}) throw std::runtime_error{"complete SysEx was altered or running status leaked"};
  auto const count{output.messages.size()};
  send({0x90, 1, 0xb1, 7, 99});
  if(output.messages.size() != count + 1 || output.messages.back() != 0x6307b1) throw std::runtime_error{"interrupted channel message leaked"};
  send({0xf1, 12, 13, 14, 0xf2, 1, 2, 3, 4, 0xf6});
  if(output.messages[output.messages.size() - 3] != 0xcf1 || output.messages[output.messages.size() - 2] != 0x201f2 || output.messages.back() != 0xf6) throw std::runtime_error{"system common message length/status changed"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
