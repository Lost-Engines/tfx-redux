#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/integer_polygon.h"

int main(int const argc, char const *const argv[]) try {
  bool const pixels{argc == 2 && std::string_view{argv[1]} == "--pixels"};
  std::ifstream fixture;if(argc == 2 && !pixels) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !pixels && !fixture)) throw std::runtime_error{"expected [FILE] or --pixels"};
  auto &input{fixture.is_open() ? fixture : std::cin};unsigned int count,bank,cases{0};
  while(input >> count >> bank) {
    vec2<int16_t> minimum,maximum;input >> minimum.x >> minimum.y >> maximum.x >> maximum.y;
    std::vector<tfx::render::indexed_vertex> vertices;
    for(unsigned int i{0}; i != count; ++i) {
      int16_t x,y;unsigned int shade;input >> x >> y >> shade;
      vertices.push_back({
        .position{x,y},
        .shade_q8{static_cast<uint16_t>(shade)},
      });
    }
    tfx::render::colour_map remap;
    for(unsigned int i{0}; i != remap.size(); ++i) remap[i] = static_cast<uint8_t>(bank ? 255 - i : i);
    tfx::render::framebuffer output;output.pixels.fill(99);
    tfx::render::integer_gouraud_polygon(output,vertices,remap,minimum,maximum);
    uint64_t hash{14695981039346656037ull};
    for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { uint64_t expected;if(!(input >> expected) || expected != hash) throw std::runtime_error{"native Gouraud polygon mismatch at case " + std::to_string(cases)}; } else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
