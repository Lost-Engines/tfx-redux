#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/horizon.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  int32_t initial;
  int16_t movement;
  while(input >> initial >> movement) {
    tfx::render::horizon_phase phase{initial};
    phase.advance(movement);
    if(argc == 2) {
      int32_t expected;
      if(!(input >> expected) || expected != phase.packed_state()) throw std::runtime_error{"native horizon phase mismatch at case " + std::to_string(cases)};
    } else {
      std::cout << phase.packed_state() << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
