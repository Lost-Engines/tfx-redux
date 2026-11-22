#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/rounded_model_polygon.h"

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) if(std::string_view{argv[i]} == "--pixels") pixels = true;else { fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open rounded model fixture"}; }
  auto &input{fixture.is_open() ? fixture : std::cin};bool wide;int mode;uint16_t seed;unsigned int cases{0};
  while(input >> wide >> mode >> seed) {
    auto view{tfx::render::original_camera({0,0,0},mat3<int32_t>{},{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};view.far_distance = 1e10;
    input >> view.viewport_minimum.x >> view.viewport_minimum.y >> view.viewport_maximum.x >> view.viewport_maximum.y;
    std::array<tfx::models::model_vertex,4> vertices;
    for(auto &v : vertices) {
      vec3<int32_t> point;input >> point.x >> point.y >> point.z;
      v.position = vec3d{point} / 65536;
    }
    tfx::render::framebuffer output;output.pixels.fill(99);
    bool const defined{tfx::render::rounded_model_polygon(output,vertices,view,17,mode,seed)};
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) { uint32_t const state{seed};std::cout.write(reinterpret_cast<char const*>(&state),sizeof(state));std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size()); } else if(fixture.is_open()) { uint64_t expected;unsigned int expected_seed;if(!defined || !(input >> expected >> expected_seed) || hash != expected || seed != expected_seed) throw std::runtime_error{"native rounded model mismatch at " + std::to_string(cases)}; } else std::cout << hash << ' ' << seed << ' ' << defined << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
