#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/model_jitter.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{fixture.is_open() ? fixture : std::cin};uint16_t x,y;unsigned int count,cases{0};
  while(input >> x >> y >> count) {
    tfx::render::model_jitter frames{{
      .x{x},
      .y{y},
    }};
    uint64_t hash{14695981039346656037ull};
    for(unsigned int i{0}; i != count; ++i) {
      bool enabled;input >> enabled >> x >> y;frames.begin(enabled);
      for(auto const value : {frames.state().x,frames.state().y}) {
        hash = (hash ^ static_cast<uint8_t>(value)) * 1099511628211;
        hash = (hash ^ static_cast<uint8_t>(value >> 8)) * 1099511628211;
      }
      frames.state() = {
        .x{x},
        .y{y},
      };
    }
    if(fixture.is_open()) { uint64_t expected;if(!(input >> expected) || expected != hash) throw std::runtime_error{"native model jitter frame mismatch at " + std::to_string(cases)}; } else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
