#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "world/model_orientation.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  vec3<tfx::math::angle> actor;unsigned int mode,heading,orientation,cases{0};
  while(input >> actor.x >> actor.y >> actor.z) {
    tfx::world::model_part part;
    input >> part.rotation.x >> part.rotation.y >> part.rotation.z >> mode >> heading >> orientation;
    part.rotation_mode = static_cast<uint8_t>(mode);
    auto const angles{tfx::world::model_orientation(actor,part,static_cast<tfx::math::angle>(heading),static_cast<tfx::world::part_orientation>(orientation))};
    std::array<unsigned int,4> const result{angles.x,angles.y,angles.z,mode ? mode : 1};
    if(fixture.is_open()) { for(auto const value : result) { unsigned int expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"native SSD orientation mismatch at " + std::to_string(cases)}; } } else { for(auto const value : result) std::cout << value << ' ';std::cout << '\n'; }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
