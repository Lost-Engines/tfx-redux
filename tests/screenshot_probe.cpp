#include <cstdlib>
#include <iostream>
#include "render/ilbm.h"

int main() {
  tfx::render::dac_palette colours;
  tfx::render::framebuffer image;
  while(std::cin.read(reinterpret_cast<char *>(colours.data()), colours.size())) {
    if(!std::cin.read(reinterpret_cast<char *>(image.pixels.data()), image.pixels.size())) return EXIT_FAILURE;
    auto const output{tfx::render::encode_ilbm(image, colours)};
    for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(output.size() >> (byte * 8)));
    std::cout.write(reinterpret_cast<char const *>(output.data()), static_cast<std::streamsize>(output.size()));
  }
  return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
}
