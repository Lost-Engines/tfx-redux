#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/model_point.h"

int main(int const argc, char const *const argv[]) try {
  bool const pixels{argc == 2 && std::string_view{argv[1]} == "--pixels"};
  std::ifstream fixture;if(argc == 2 && !pixels) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !pixels && !fixture)) throw std::runtime_error{"expected [FILE] or --pixels"};
  auto &input{fixture.is_open() ? fixture : std::cin};bool wide;unsigned int size,colour,cases{0};
  while(input >> wide >> size >> colour) {
    tfx::render::camera view;
    input >> view.projection_origin.x >> view.projection_origin.y >> view.viewport_minimum.x >> view.viewport_minimum.y >> view.viewport_maximum.x >> view.viewport_maximum.y;
    view.opening = wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal;
    vec3<int32_t> point;input >> point.x >> point.y >> point.z;
    tfx::render::framebuffer output;output.pixels.fill(99);
    if(!tfx::render::model_point(output,point,view,size,static_cast<uint8_t>(colour))) throw std::runtime_error{"native point IDIV overflow"};
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { uint64_t expected;if(!(input >> expected) || expected != hash) throw std::runtime_error{"native distant model point mismatch at case " + std::to_string(cases)}; } else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
