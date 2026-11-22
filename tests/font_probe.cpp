#include <cstdlib>
#include <iostream>
#include "presentation/fonts.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"font_probe TFX.DAT"};
  tfx::presentation::fonts const fonts{tfx::resources::read_file(argv[1])};
  unsigned int style{0},colour{0};int x{0},y{0};std::string hex;
  auto const word{[](auto const value){ for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> shift)); }};
  while(std::cin >> style >> x >> y >> colour >> hex) {
    std::string text;
    if(hex != "-") for(unsigned int i{0}; i < hex.size(); i += 2) text.push_back(static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16)));
    tfx::render::framebuffer output,mask;
    fonts.draw(output,static_cast<uint8_t>(style),{x,y},text,static_cast<uint8_t>(colour),&mask);
    word(fonts.measure(static_cast<uint8_t>(style),text));word(fonts.line_height(static_cast<uint8_t>(style)));
    for(auto const *frame : {&output,&mask}) std::cout.write(reinterpret_cast<char const*>(frame->pixels.data()),static_cast<std::streamsize>(frame->pixels.size()));
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
