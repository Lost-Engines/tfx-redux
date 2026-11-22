#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/display_pages.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected optional display-page fixture"};
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open display-page fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::render::display_pages pages;
  unsigned int cases{0};
  unsigned int phase;
  bool reset;
  int16_t offset;
  while(input >> reset >> phase >> offset) {
    if(phase > 255) throw std::runtime_error{"refresh phase exceeds its original byte"};
    if(reset) pages.reset();
    auto remaining{static_cast<uint8_t>(phase)};
    bool const copy{pages.refresh_panel(remaining, offset)};
    for(unsigned int const value : {static_cast<unsigned int>(copy), static_cast<unsigned int>(remaining)}) {
      if(argc == 2) {
        unsigned int expected;
        if(!(input >> expected) || expected != value) throw std::runtime_error{"display-page mismatch at case " + std::to_string(cases)};
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
