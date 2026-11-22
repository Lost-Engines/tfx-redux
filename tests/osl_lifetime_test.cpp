#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include "audio/osl_sequencer.h"

namespace {
tfx::resources::bytes silent_bank() {
  tfx::resources::bytes bytes(0x500);
  auto const word{[&](unsigned int const at, uint16_t const value){
    bytes[at] = static_cast<uint8_t>(value);bytes[at + 1] = static_cast<uint8_t>(value >> 8);
  }};
  auto const pointer{[&](unsigned int const at, uint16_t const value){ word(at,value);word(at + 2,0); }};
  bytes[0] = 'O';bytes[1] = 'S';bytes[2] = 'L';bytes[3] = '1';bytes[7] = 4;
  word(0x26,1);pointer(0x48,0x80);
  word(0x92,1);word(0x94,1);
  std::ranges::fill(std::span{bytes}.subspan(0x96,16),127);
  word(0xaa,50);bytes[0xac] = 1;word(0xce,1);pointer(0x1d0,0x200);word(0x202,4);
  return bytes;
}
} // anonymous namespace
int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected original synthetic OSL lifetime fixture"};
  std::ifstream expected{argv[1]};
  if(!expected) throw std::runtime_error{"cannot read OSL lifetime fixture"};
  tfx::audio::osl_sequencer player;
  auto const snapshot{[&](unsigned int const count){
    std::ostringstream row;
    for(unsigned int index{0}; index != count; ++index) {
      auto const &state{player.state(index)};
      for(auto const value : std::array<unsigned int,8>{state.flags,state.channel_count,state.speed,state.clock_rate,state.subtick,state.order,state.loop_order,state.row}) row << value << ' ';
    }
    row << std::ranges::count_if(player.channel_states(),[](auto const &channel){ return channel.allocation != 0; });
    std::string wanted;
    if(!std::getline(expected,wanted) || row.str() != wanted) throw std::runtime_error{"OSL lifetime/state differs from original"};
  }};
  auto const ticks{[&](unsigned int const count){ for(unsigned int index{0}; index != count; ++index) player.tick(); }};
  auto const resource{player.load(silent_bank())};
  if(resource != 0 || player.play(resource) != 0) throw std::runtime_error{"initial OSL identities changed"};
  snapshot(1);
  ticks(3);snapshot(1);
  player.release(resource);snapshot(1);
  ticks(8);snapshot(1);
  if(player.load(silent_bank()) != resource) throw std::runtime_error{"released OSL slot was not reused"};
  snapshot(1);
  if(player.try_play(resource) != 1) throw std::runtime_error{"active playback was overwritten by resource-handle reuse"};
  snapshot(2);
  ticks(3);snapshot(2);
  player.stop(0);snapshot(2);
  player.stop(1);snapshot(2);
  player.release(resource);snapshot(2);
  if(player.load(silent_bank()) != resource) throw std::runtime_error{"second resource reuse failed"};
  for(unsigned int index{0}; index != tfx::audio::osl_playing_slots; ++index) {
    if(player.try_play(resource) != index) throw std::runtime_error{"OSL playback allocation order changed"};
  }
  if(player.try_play(resource)) throw std::runtime_error{"full OSL playback pool accepted another song"};
  snapshot(tfx::audio::osl_playing_slots);
  player.stop_all();snapshot(tfx::audio::osl_playing_slots);
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
