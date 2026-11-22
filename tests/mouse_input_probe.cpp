#include <cstdlib>
#include <fstream>
#include <iostream>
#include "flight/mouse_input.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::flight::mouse_flight_input state;
  int x,y,reset,initial_x,initial_y;
  unsigned int cases{0};
  while(input >> x >> y >> reset >> initial_x >> initial_y) {
    if(reset) state.position = {static_cast<int16_t>(initial_x),static_cast<int16_t>(initial_y)};
    auto const result{state.advance({static_cast<int16_t>(x),static_cast<int16_t>(y)})};
    for(auto const value : result) {
      if(argc == 2) { int expected; if(!(input >> expected) || expected != value) throw std::runtime_error{"mouse input mismatch in case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
