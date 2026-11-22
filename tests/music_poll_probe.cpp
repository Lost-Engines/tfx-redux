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
  unsigned int cases{0},mode,sound,enabled,music,cd,pending,inhibited,collision,disabled,engines,throttle,previous,last,engine,playing;
  int resume;
  while(input >> mode >> sound >> enabled >> music >> cd >> pending >> inhibited >> collision >> disabled >> engines >> throttle >> previous >> last >> resume >> engine >> playing) {
    tfx::audio::music_control_state state{
      .last_cue{static_cast<uint8_t>(last)},
      .resume_cue{static_cast<int16_t>(resume)},
      .engine_playing{engine != 0},
      .previous_throttle{std::bit_cast<float>(previous)}
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
      .engines_running{engines != 0},
      .throttle{std::bit_cast<float>(throttle)}
    };
    for(auto &order : context.lapc_orders) { unsigned int value;input >> value;order = static_cast<uint8_t>(value); }
    auto const result{tfx::audio::update_flight_music(state,context,playing != 0)};
    std::vector<int64_t> values{state.last_cue,state.resume_cue,state.engine_playing,std::bit_cast<uint32_t>(state.previous_throttle),result.stop_and_release,result.selection.has_value()};
    if(result.selection) values.insert(values.end(),{result.selection->replace_resource,result.selection->resource_index,result.selection->order,result.selection->stop_previous});
    for(auto const value : values) {
      if(argc == 2) { int64_t expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"music poll mismatch at case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
