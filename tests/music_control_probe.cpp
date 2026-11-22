#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "audio/music_control.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int mode;
  unsigned int sound;
  unsigned int enabled;
  unsigned int music;
  unsigned int cd;
  unsigned int pending;
  unsigned int inhibited;
  unsigned int collision;
  unsigned int disabled;
  unsigned int throttle;
  unsigned int cue;
  unsigned int last;
  unsigned int engine;
  int resume;
  while(input >> mode >> sound >> enabled >> music >> cd >> pending >> inhibited >> collision >> disabled >> throttle >> cue >> last >> resume >> engine) {
    tfx::audio::music_control_state state{
      .last_cue{static_cast<uint8_t>(last)},
      .resume_cue{static_cast<int16_t>(resume)},
      .engine_playing{engine != 0}
    };
    tfx::audio::music_control_context context{
      .mode{static_cast<tfx::audio::music_mode>(mode)},
      .sound_device{static_cast<uint8_t>(sound)},
      .enabled{enabled != 0},
      .music_playing{music != 0},
      .cd_playing{cd != 0},
      .preselected{pending != 0},
      .inhibited{inhibited != 0},
      .collision{collision != 0},
      .aircraft_disabled{disabled != 0},
      .throttle{std::bit_cast<float>(throttle)}
    };
    for(auto &order : context.lapc_orders) {
      unsigned int value;
      input >> value;
      order = static_cast<uint8_t>(value);
    }
    auto const result{tfx::audio::select_flight_music(state, context, static_cast<uint8_t>(cue))};
    std::vector<int> values{state.last_cue, state.resume_cue, state.engine_playing, result.has_value()};
    if(result) values.insert(values.end(), {result->replace_resource, result->resource_index, result->order, result->stop_previous});
    for(auto const value : values) {
      if(argc == 2) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"music controller mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << value << ' ';
      }
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
