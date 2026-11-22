#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/point_projection.h"

int main(int const argc, char const *const argv[]) try {
  bool const wide{argc > 1 && std::string_view{argv[1]} == "--wide"};
  int const count{argc - static_cast<int>(wide)};
  int const path_index{wide ? 2 : 1};
  std::ifstream fixture;
  if(count == 2) fixture.open(argv[path_index]);
  if(count > 2 || (count == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{count == 2 ? fixture : std::cin};
  unsigned int cases{0};
  int16_t radius;
  while(input >> radius) {
    tfx::render::point_projection projection;
    if(wide) projection.opening = tfx::render::aperture::missile_nose;
    vec3<int32_t> point;
    for(auto &value : projection.matrix.data) input >> value;
    input >> projection.position_q16.x >> projection.position_q16.y >> projection.position_q16.z >> point.x >> point.y >> point.z >> projection.origin.x >> projection.origin.y >> projection.maximum.x >> projection.maximum.y;
    if(!input) throw std::runtime_error{"incomplete visibility input"};
    bool const visible{projection.sphere_visible(point, radius)};
    if(count == 2) {
      bool expected;
      if(!(input >> expected) || visible != expected) throw std::runtime_error{"native sphere visibility differs at case " + std::to_string(cases)};
    } else {
      std::cout << visible << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
