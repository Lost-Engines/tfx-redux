#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "missions/engagement.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int permitted;
  unsigned int kind;
  unsigned int player;
  unsigned int aircraft;
  unsigned int arcade;
  unsigned int training;
  unsigned int inhibited;
  unsigned int frames;
  int notice;
  while(input >> permitted >> notice >> kind >> player >> aircraft >> arcade >> training >> inhibited >> frames) {
    tfx::missions::engagement_state state{
      .permitted{permitted != 0},
      .notice_frames{static_cast<int16_t>(notice)},
    };
    tfx::missions::engagement_context const context{
      .aircraft{static_cast<tfx::flight::player_aircraft>(aircraft)},
      .arcade{arcade != 0},
      .training{training != 0},
      .inhibited{inhibited != 0},
    };
    auto const events{state.observe(static_cast<tfx::missions::engagement_warning>(kind), player != 0, context)};
    for(unsigned int frame{0}; frame != frames; ++frame) state.finish_frame();
    std::vector<int> values{state.permitted, state.notice_frames, state.authorised(context), static_cast<int>(events.size())};
    for(auto const &event : events) {
      if(auto const sample{std::get_if<tfx::missions::engagement_sample>(&event)}) {
        values.insert(values.end(), {1, sample->urgent ? 2 : 1, static_cast<int>(sample->sample)});
      } else {
        values.push_back(0);
      }
    }
    for(auto const value : values) {
      if(argc == 2) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"engagement mismatch at case " + std::to_string(cases)};
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
