#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/joystick.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::flight::joystick_buttons buttons;
  int x,y;
  unsigned int scale,mask,type,ticks,reset,cases{0};
  while(input >> x >> y >> scale >> mask >> type >> ticks >> reset) {
    if(reset) buttons = {};
    auto const axes{tfx::flight::joystick_axes({static_cast<float>(x) / 4096.0f,static_cast<float>(y) / 4096.0f},std::bit_cast<float>(scale))};
    auto const action{buttons.advance(static_cast<uint8_t>(mask),static_cast<uint8_t>(type),static_cast<tfx::simulation::timer_ticks>(ticks))};
    std::vector<int> values{axes.x,axes.y,static_cast<int>(action)};
    for(auto const &profile : buttons.remaining) for(auto const value : profile) values.push_back(value);
    for(auto const value : values) {
      if(argc == 2) { int expected; if(!(input >> expected) || expected != value) throw std::runtime_error{"joystick mismatch in case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
