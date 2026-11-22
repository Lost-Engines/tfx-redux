#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/glare.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0},view;
  tfx::render::glare_context context;
  tfx::render::point_projection projection;
  while(input >> context.flak_flash >> context.cloud_layer >> context.layer_y >> view) {
    input >> context.camera_anchor.x >> context.camera_anchor.y >> context.camera_anchor.z >> context.minimum.x >> context.minimum.y >> context.maximum.x >> context.maximum.y;
    for(auto &value : projection.matrix.data) input >> value;
    input >> projection.position_q16.x >> projection.position_q16.y >> projection.position_q16.z >> projection.origin.x >> projection.origin.y;
    if(!input) throw std::runtime_error{"incomplete sunlight-glare input"};
    context.view = static_cast<tfx::cockpit::view_mode>(view);
    unsigned int const level{tfx::render::glare_level(projection,context)};
    if(argc == 2) {
      unsigned int expected;
      if(!(input >> expected) || level != expected) throw std::runtime_error{"native glare mismatch at case " + std::to_string(cases)};
    } else std::cout << level << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
