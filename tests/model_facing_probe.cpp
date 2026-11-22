#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/integer_model_polygon.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};bool wide;double scale;vec2<int16_t> origin;
  mat3<int32_t> const identity{32767,0,0,0,32767,0,0,0,32767};
  while(input >> wide >> scale >> origin.x >> origin.y) {
    auto const view{tfx::render::original_camera({0,0,0},identity,vec2d{origin},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    std::vector<tfx::models::model_vertex> vertices;
    for(unsigned int i{0}; i != 3; ++i) {
      vec3<int32_t> point;input >> point.x >> point.y >> point.z;
      vertices.push_back({
        .position{vec3d{point} / 65536 * scale},
        .uv{0,0},
        .projection_scale{scale},
      });
    }
    auto const branch{tfx::render::model_facing(vertices,view,tfx::models::facing_test::branch)};
    if(!branch) throw std::runtime_error{"unhandled model-facing branch at case " + std::to_string(cases)};
    if(argc == 2) { bool expected;if(!(input >> expected) || expected != *branch) throw std::runtime_error{"native model-facing branch mismatch at case " + std::to_string(cases)}; } else std::cout << *branch << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
