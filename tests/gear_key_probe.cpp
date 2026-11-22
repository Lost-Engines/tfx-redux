#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "flight/gear_controls.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected optional gear-key fixture"};
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open gear-key fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int craft;
  unsigned int condition;
  unsigned int cases{0};
  int32_t speed;
  bool down;
  bool inhibited;
  bool crashed;
  bool disabled;
  bool landing;
  bool landed;
  while(input >> craft >> speed >> down >> condition >> inhibited >> crashed >> disabled >> landing >> landed) {
    auto const result{tfx::flight::press_gear_key({
      .aircraft{static_cast<tfx::flight::player_aircraft>(craft + 1)},         // Native 0..2 -> public 1..3.
      .speed{speed},
      .gear_down{down},
      .condition{static_cast<tfx::flight::system_condition>(condition)},
      .inhibited{inhibited},
      .crashed{crashed},
      .disabled{disabled},
      .automatic_landing{landing},
      .landed{landed},
    })};
    bool const changed{result.gear_down.has_value()};
    std::array<unsigned int, 6> const values{
      result.speed_warning, result.gear_down.value_or(down), changed ? (*result.gear_down ? 1u : 2u) : condition,
      changed ? 1u : 0u, changed ? 5u : 0u, changed ? (*result.gear_down ? 3u : 4u) : 0u,
    };
    for(auto const value : values) {
      if(argc == 2) {
        unsigned int expected;
        if(!(input >> expected) || expected != value) throw std::runtime_error{"gear-key mismatch at case " + std::to_string(cases)};
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
