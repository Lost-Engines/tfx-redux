#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "math/native.h"
#include "fixture_check.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot open anchor fixture"};
  unsigned int pitch{0};
  unsigned int heading{0};
  unsigned int roll{0};
  unsigned int cases{0};
  int32_t x{0};
  int32_t y{0};
  int32_t z{0};
  int ax{0};
  int ay{0};
  int az{0};
  while(input >> x >> y >> z >> pitch >> heading >> roll >> ax >> ay >> az) {
    auto const position{tfx::math::place_anchor({x, y, z}, {static_cast<uint16_t>(pitch), static_cast<uint16_t>(heading), static_cast<uint16_t>(roll)}, {static_cast<int16_t>(ax), static_cast<int16_t>(ay), static_cast<int16_t>(az)})};
    if(argc == 2) {
      if(!tfx::test::fixture_matches(input, position.x) || !tfx::test::fixture_matches(input, position.y) || !tfx::test::fixture_matches(input, position.z)) throw std::runtime_error{"native anchor transform differs in case " + std::to_string(cases)};
    } else {
      std::cout << position.x << ' ' << position.y << ' ' << position.z << '\n';
    }
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"incomplete anchor input"};
  if(argc == 2) std::cout << cases << " native anchor transforms passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
