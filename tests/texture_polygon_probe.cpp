#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/integer_texture_polygon.h"

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) if(std::string_view{argv[i]} == "--pixels") pixels = true;else { fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open texture polygon fixture"}; }
  auto &input{fixture.is_open() ? fixture : std::cin};unsigned int count,offset,cases{0};vec2<int16_t> minimum,maximum;
  std::array<uint8_t,65536> texture;
  for(unsigned int v{0}; v != 256; ++v) for(unsigned int u{0}; u != 256; ++u) texture[v * 256 + u] = static_cast<uint8_t>(u * 3 + v * 5 + (u ^ v));
  while(input >> count >> offset >> minimum.x >> minimum.y >> maximum.x >> maximum.y) {
    std::vector<tfx::render::textured_vertex> vertices(count);
    for(auto &v : vertices) input >> v.position.x >> v.position.y >> v.uv_q7.x >> v.uv_q7.y;
    tfx::render::colour_map remap;for(unsigned int i{0}; i != remap.size(); ++i) remap[i] = static_cast<uint8_t>(i * 5 + offset);
    tfx::render::framebuffer output;output.pixels.fill(99);
    tfx::render::integer_texture_polygon(output,vertices,texture,remap,minimum,maximum);
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { uint64_t expected;if(!(input >> expected) || hash != expected) throw std::runtime_error{"native textured polygon mismatch at " + std::to_string(cases)}; } else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
