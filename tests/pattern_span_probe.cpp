#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/pattern_raster.h"

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};
  std::ifstream fixture;
  for(int i{1}; i != argc; ++i) {
    if(std::string_view{argv[i]} == "--pixels") {
      pixels = true;
    } else {
      fixture.open(argv[i]);
      if(!fixture) throw std::runtime_error{"cannot open pattern-span fixture"};
    }
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  int mode;
  unsigned int seed;
  unsigned int ink;
  unsigned int bottom;
  unsigned int count;
  unsigned int background;
  unsigned int cases{0};
  while(input >> mode >> seed >> ink >> bottom >> count >> background) {
    std::vector<tfx::render::polygon_span> spans;
    spans.reserve(count);
    for(unsigned int i{0}; i != count; ++i) {
      int16_t left;
      int16_t right;
      input >> left >> right;
      spans.push_back({
        .row{static_cast<int16_t>(bottom + 1 - count + i)},
        .left{left},
        .right{right},
      });
    }
    tfx::render::framebuffer output;
    for(unsigned int y{0}; y != tfx::render::screen_height; ++y) {
      for(unsigned int x{0}; x != tfx::render::screen_width; ++x) output.pixels[y * tfx::render::screen_width + x] = background ? static_cast<uint8_t>(x * 3 + y * 5 + (x ^ y)) : 99;
    }
    auto state{static_cast<uint16_t>(seed)};
    tfx::render::pattern_polygon(output, state, spans, static_cast<uint8_t>(ink), mode);
    uint64_t hash{14695981039346656037ull};
    for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) {
      std::cout.write(reinterpret_cast<char const *>(output.pixels.data()), output.pixels.size());
    } else if(fixture.is_open()) {
      uint64_t expected;
      unsigned int expected_seed;
      if(!(input >> expected >> expected_seed) || hash != expected || state != expected_seed) throw std::runtime_error{"native pattern-span mismatch at " + std::to_string(cases)};
    } else {
      std::cout << hash << ' ' << state << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
