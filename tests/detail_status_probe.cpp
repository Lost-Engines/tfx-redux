#include <cstdlib>
#include <fstream>
#include <iostream>
#include "world/environment.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int status,detail,cases{0};
  while(input >> status >> detail) {
    auto const actual{tfx::world::apply_detail_status(static_cast<uint16_t>(status),static_cast<uint8_t>(detail))};
    if(argc == 2) { unsigned int expected;if(!(input >> expected) || expected != actual) throw std::runtime_error{"detail status mismatch"}; } else std::cout << actual << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
