#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/fonts.h"

namespace {

void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
tfx::resources::bytes test_fonts() {
  // Test-authored tables use the native layout, without original glyph bytes.
  unsigned int constexpr data{973476};
  unsigned int constexpr code{0x70aa4};
  tfx::resources::bytes image(data + 0xe100);
  auto const word{[&](unsigned int const at, uint32_t const value){
    for(unsigned int byte{0}; byte != 4; ++byte) image.at(at + byte) = static_cast<uint8_t>(value >> (byte * 8));
  }};
  for(unsigned int i{0}; i != 64; ++i) image.at(data + 0xcc2f + i) = 255;
  image.at(data + 0xcc2f + 1) = 0;
  image.at(data + 0xcc6f) = 3;
  word(data + 0xcdc0, 0xe000);
  image.at(data + 0xe000) = 3;
  image.at(data + 0xe001) = 2;
  image.at(data + 0xe002) = 160;
  image.at(data + 0xe003) = 64;
  image.at(data + 0xd485) = 3;
  word(data + 0xd300, 0xe010);
  image.at(data + 0xe010) = 2;
  image.at(data + 0xe011) = 3;
  image.at(data + 0xe012) = 192;
  image.at(data + 0xe013) = 64;
  image.at(data + 0xe014) = 192;
  image.at(code + 0x5bb51 + 1) = 5;
  image.at(code + 0x5bb51 + 9) = 3;
  image.at(code + 0x63f02 + 1) = 5;
  return image;
}

} // anonymous namespace

int main() try {
  tfx::presentation::fonts const fonts{test_fonts()};
  tfx::render::framebuffer output;
  tfx::render::framebuffer coverage;
  output.pixels.fill(99);
  fonts.draw(output, 3, {2, 3}, "Aa", 0, &coverage);
  require(output.pixels[3 * 320 + 2] == 0 && coverage.pixels[3 * 320 + 2] == 255 && output.pixels[3 * 320 + 3] == 99, "black ink must overwrite its glyph pixels and preserve its holes");
  require(coverage.pixels[3 * 320 + 6] == 255 && fonts.measure(3, "Aa") == 4, "native draw advances must remain separate from case-sensitive measurement");
  require(fonts.line_height(3) == 16 && fonts.measure(0, "") == -2, "native line height or final-spacing trim differs");
  output.pixels.fill(0);
  coverage.pixels.fill(0);
  fonts.draw(output, 6, {10, 10}, std::string(1, static_cast<char>(129)), 127, &coverage);
  require(coverage.pixels[8 * 320 + 10] == 255 && fonts.measure(6, std::string(1, static_cast<char>(129))) == 3 && fonts.glyph(6, 129).advance == 4, "extended small-font baseline or separate width metric differs");
  output.pixels.fill(0);
  coverage.pixels.fill(0);
  fonts.draw(output, 1, {0, 0}, "A@A", 127, &coverage);
  require(coverage.pixels[0] == 255 && coverage.pixels[2] == 255 && coverage.pixels[16] == 0 && fonts.measure(1, "A@A") == 24, "fixed-font low-six-bit terminator differs");
  output.pixels.fill(0);
  coverage.pixels.fill(0);
  fonts.draw(output, 1, {0, 0}, "AIA", 127, &coverage);
  require(coverage.pixels[12] == 255 && coverage.pixels[16] == 0, "following uppercase I must use the native narrow fixed cell");
  require(!fonts.glyph(0, 'A').available, "null native glyph pointer must remain explicitly unavailable");
  std::cout << "Owned font tables, black coverage, case-sensitive metrics, extended baseline and fixed terminators passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
