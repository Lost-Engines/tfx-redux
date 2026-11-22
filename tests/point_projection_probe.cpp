#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/point_projection.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open native point-projection fixture"};
  } else if(argc != 1) throw std::runtime_error{"point projection probe accepts an optional fixture"};
  std::istream &input{argc == 2 ? fixture : std::cin};
  unsigned int mode, cases{0};
  while(input >> mode) {
    tfx::render::point_projection projection;
    vec3<int32_t> point;
    for(auto &value : projection.matrix.data) input >> value;
    input >> projection.position_q16.x >> projection.position_q16.y >> projection.position_q16.z >> point.x >> point.y >> point.z >> projection.origin.x >> projection.origin.y;
    if(!input) throw std::runtime_error{"incomplete point-projection input"};
    auto const screen{mode ? projection.fixed(point) : projection.integer(point)};
    int const valid{screen.has_value()}, x{screen ? screen->x : -1}, y{screen ? screen->y : -1};
    if(argc == 2) {
      int expected_valid, expected_x, expected_y;
      input >> expected_valid >> expected_x >> expected_y;
      if(!input || valid != expected_valid || x != expected_x || y != expected_y) throw std::runtime_error{"native point-projection fixture differs"};
    } else std::cout << valid << ' ' << x << ' ' << y << '\n';
    ++cases;
  }
  if(argc == 2) std::cout << cases << " native point-projection fixtures passed\n";
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
