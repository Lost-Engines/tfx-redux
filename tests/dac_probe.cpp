#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/framebuffer.h"

int main(int const argc, char const *const argv[]) {
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open native DAC fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int red;
  unsigned int green;
  unsigned int blue;
  while(input >> red >> green >> blue) {
    tfx::render::dac_palette raw;
    for(unsigned int index{0}; index != 256; ++index) {
      raw[index * 3] = static_cast<uint8_t>(red);
      raw[index * 3 + 1] = static_cast<uint8_t>(green);
      raw[index * 3 + 2] = static_cast<uint8_t>(blue);
    }
    auto const colours{tfx::render::expand_dac_palette(raw)};
    if(argc == 2) {
      unsigned int expected_red;
      unsigned int expected_green;
      unsigned int expected_blue;
      if(!(input >> expected_red >> expected_green >> expected_blue)) throw std::runtime_error{"incomplete native DAC fixture"};
      for(auto const &colour : colours) {
        if(colour.r != expected_red || colour.g != expected_green || colour.b != expected_blue) throw std::runtime_error{"native DAC fixture differs"};
      }
    } else {
      std::cout << static_cast<unsigned int>(colours[0].r) << ' ' << static_cast<unsigned int>(colours[0].g) << ' ' << static_cast<unsigned int>(colours[0].b) << '\n';
    }
    if(red > 63 || green > 63 || blue > 63) {
      bool rejected{false};
      try {
        tfx::render::decode_palette(raw);
      } catch(std::runtime_error const &) {
        rejected = true;
      }
      if(!rejected) throw std::runtime_error{"authored palette validation accepted a non-DAC component"};
    }
  }
}
