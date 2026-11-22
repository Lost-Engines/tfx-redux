#include <cstdlib>
#include <iostream>
#include "presentation/widget_font.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"widget_font_probe TFX_DAT"};
  tfx::presentation::widget_font const font{tfx::resources::read_file(argv[1])};
  int x{0};
  int y{0};
  unsigned int colour{0};
  std::string hex;
  while(std::cin >> x >> y >> colour >> hex) {
    std::string text;
    if(hex != "-") {
      for(unsigned int at{0}; at < hex.size(); at += 2) text.push_back(static_cast<char>(std::stoul(hex.substr(at, 2), nullptr, 16)));
    }
    tfx::render::framebuffer frame;
    tfx::render::framebuffer coverage;
    auto const width{static_cast<uint32_t>(font.measure(text))};
    for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(width >> shift));
    font.draw(frame, {x, y}, text, static_cast<uint8_t>(colour), &coverage);
    for(auto const *image : {&frame, &coverage}) std::cout.write(reinterpret_cast<char const *>(image->pixels.data()), image->pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
