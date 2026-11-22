#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/camera.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};bool wide;vec3<int32_t> point;double scale;
  mat3<int32_t> const identity{32767,0,0,0,32767,0,0,0,32767};
  while(input >> wide >> point.x >> point.y >> point.z >> scale) {
    auto const camera{tfx::render::original_camera({0,0,0},identity,{105,75},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    auto const screen{camera.project(vec3d{point} / 65536 * scale,scale)};
    int const x{static_cast<int>(screen.x)},y{static_cast<int>(screen.y)};
    if(argc == 2) { int ex,ey;if(!(input >> ex >> ey) || ex != x || ey != y) throw std::runtime_error{"native model aperture mismatch at case " + std::to_string(cases)}; } else std::cout << x << ' ' << y << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
