#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "missions/timeline.h"

namespace {

uint64_t run(std::istream &input, bool const emit) {
  unsigned int messages{0},events{0},steps{0};
  uint32_t elapsed{0},clock{0};
  if(!(input >> messages >> events >> steps >> elapsed >> clock)) throw std::runtime_error{"invalid mission timeline fixture"};
  tfx::missions::setup source{};
  source.clock = std::bit_cast<int32_t>(clock);
  for(unsigned int index{0}; index != messages; ++index) {
    uint32_t time{0};
    input >> time;
    source.messages.push_back({
      .time{std::bit_cast<int32_t>(time)},
      .text{},
    });
  }
  for(unsigned int index{0}; index != events; ++index) {
    uint32_t time{0};
    input >> time;
    source.scenery_events.push_back({
      .time{std::bit_cast<int32_t>(time)},
      .entry{0},
      .cell{0,0},
    });
  }
  tfx::missions::timeline state{source};
  state.reset(elapsed);
  uint64_t digest{14695981039346656037ULL};
  auto const word{[&](uint32_t const value){
    if(emit) std::cout << value << ' ';
    for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((value >> shift) & 255)) * 1099511628211ULL;
  }};
  for(unsigned int step{0}; step != steps; ++step) {
    unsigned int ticks{0};
    input >> ticks;
    auto const due_messages{state.begin_frame()};
    state.advance_clock(static_cast<uint16_t>(ticks));
    // HUD consumers observe the advanced clock before scenery events run.
    auto const hud_elapsed{state.elapsed()}, hud_time{state.time_of_day()};
    auto const due_events{state.finish_frame()};
    word(hud_elapsed);
    word(hud_time);
    word(static_cast<uint32_t>(due_messages.size()));
    for(auto const index : due_messages) word(index);
    word(static_cast<uint32_t>(due_events.size()));
    for(auto const index : due_events) word(index);
  }
  if(emit) std::cout << '\n';
  return digest;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  bool const check{argc == 3 && std::string_view{argv[1]} == "--check"};
  if(argc != 1 && !check) throw std::runtime_error{"timeline_probe [--check FIXTURE]"};
  if(check) {
    std::ifstream input{argv[2]};
    if(!input) throw std::runtime_error{"cannot open mission timeline fixture"};
    unsigned int cases{0};
    while(input >> std::ws && input.peek() != std::char_traits<char>::eof()) {
      auto const digest{run(input,false)};
      uint64_t expected{0};
      if(!(input >> expected) || digest != expected) throw std::runtime_error{"native mission timeline differs in case " + std::to_string(cases)};
      ++cases;
    }
    if(!cases || !input.eof()) throw std::runtime_error{"incomplete mission timeline fixture"};
    std::cout << cases << " native mission timelines passed" << std::endl;
  } else {
    while(std::cin >> std::ws && std::cin.peek() != std::char_traits<char>::eof()) run(std::cin,true);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
