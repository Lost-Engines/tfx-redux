#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "world/model_placement.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool wide;unsigned int placement,cases{0};
  while(input >> placement >> wide) {
    tfx::render::point_projection view;
    for(auto &value : view.matrix.data) input >> value;
    input >> view.position_q16.x >> view.position_q16.y >> view.position_q16.z >> view.origin.x >> view.origin.y >> view.maximum.x >> view.maximum.y;
    view.opening = wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal;
    vec3<int32_t> origin;vec3<int16_t> anchor;tfx::math::angle heading;int16_t radius;
    input >> origin.x >> origin.y >> origin.z >> anchor.x >> anchor.y >> anchor.z >> heading >> radius;
    auto const result{tfx::world::place_model_part(view,origin,anchor,heading,static_cast<tfx::world::part_position>(placement),radius)};
    int32_t const values[]{result.position.x,result.position.y,result.position.z,result.camera_origin.x,result.camera_origin.y,result.camera_origin.z,result.visible};
    if(fixture.is_open()) { for(auto const value : values) { int32_t expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"native model placement mismatch at case " + std::to_string(cases)}; } } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
