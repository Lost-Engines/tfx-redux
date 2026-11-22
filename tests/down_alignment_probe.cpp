#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "math/attitude.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"down-axis probe accepts an optional native fixture"};
  std::ifstream fixture;
  if(argc == 2) { fixture.open(argv[1]);if(!fixture) throw std::runtime_error{"cannot open down-axis fixture"}; }
  std::istream &input{argc == 2 ? fixture : std::cin};
  tfx::math::attitude basis;unsigned int count{0};
  while(input >> basis.right.x) {
    input >> basis.down.x >> basis.forward.x;
    for(unsigned int row{1}; row != 3; ++row) input >> basis.right[row] >> basis.down[row] >> basis.forward[row];
    vec3f direction;input >> direction.x >> direction.y >> direction.z;
    if(!input) throw std::runtime_error{"incomplete down-axis input"};
    bool const changed{basis.align_down(direction)};
    std::array<int32_t,10> const actual{changed,basis.right.x,basis.down.x,basis.forward.x,basis.right.y,basis.down.y,basis.forward.y,basis.right.z,basis.down.z,basis.forward.z};
    for(auto const value : actual) {
      if(argc == 2) {
        int32_t expected;input >> expected;
        if(!input || value != expected) throw std::runtime_error{"native down-axis fixture differs at case " + std::to_string(count)};
      } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++count;
  }
  if(argc == 2) { if(!count) throw std::runtime_error{"empty down-axis fixture"};std::cout << count << " native down-axis fixtures passed\n"; }
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
