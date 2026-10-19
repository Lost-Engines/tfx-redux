#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "render/ilbm.h"

namespace {

auto require(bool const condition, char const *const message)->void {
  if(!condition) throw std::runtime_error{message};
}
auto decode(std::vector<uint8_t> const &file)->tfx::render::framebuffer {
  auto const word{[&](unsigned int const offset, unsigned int const bytes) {
    uint32_t value{0};
    for(unsigned int index{0}; index != bytes; ++index) value = value << 8 | file.at(offset + index);
    return value;
  }};
  require(file.size() == word(4,4) + 8,"FORM size does not cover the file");
  require(std::string{file.begin(),file.begin() + 4} == "FORM" && std::string{file.begin() + 8,file.begin() + 12} == "ILBM","capture is not an ILBM");
  require(file[28] == 8 && file[30] == 1,"plane count or compression changed");
  require(file[34] == 1 && file[35] == 1,"square-pixel aspect is missing");
  require(file.size() == word(820,4) + 824,"BODY size is incorrect");
  tfx::render::framebuffer image;
  image.resize({static_cast<int>(word(20,2)),static_cast<int>(word(22,2))});
  unsigned int position{824};
  for(unsigned int y{0}; y != static_cast<unsigned int>(image.size.y); ++y) {
    for(unsigned int plane{0}; plane != 8; ++plane) {
      std::vector<uint8_t> row(static_cast<unsigned int>((image.size.x + 15) / 16 * 2));
      unsigned int decoded{0};
      while(decoded != row.size()) {
        int const control{std::bit_cast<int8_t>(file.at(position++))};
        unsigned int const count{static_cast<unsigned int>(control < 0 ? 1 - control : control + 1)};
        require(control != -128 && decoded + count <= row.size(),"compressed packet crossed a plane row");
        uint8_t const repeated{control < 0 ? file.at(position++) : uint8_t{0}};
        for(unsigned int index{0}; index != count; ++index) row[decoded++] = control < 0 ? repeated : file.at(position++);
      }
      for(unsigned int x{0}; x != static_cast<unsigned int>(image.size.x); ++x) image.pixels[y * static_cast<unsigned int>(image.size.x) + x] |= static_cast<uint8_t>(((row[x / 8] >> (7 - x % 8)) & 1u) << plane);
    }
  }
  require(position == file.size(),"unexpected data follows the compressed image");
  return image;
}

} // anonymous namespace

int main() try {
  tfx::render::framebuffer image;
  tfx::render::dac_palette colours;
  for(unsigned int index{0}; index != colours.size(); ++index) colours[index] = static_cast<uint8_t>(index);
  auto const blank{tfx::render::encode_ilbm(image,colours)};
  require(blank.size() == 4024 && blank[824] == 217 && blank[825] == 0,"blank bitplane rows must be a single repeated packet");
  for(unsigned int index{0}; index != colours.size(); ++index) require(blank[48 + index] == (colours[index] & 63) * 4,"VGA palette mask or original eight-bit conversion changed");
  for(unsigned int pattern{0}; pattern != 4; ++pattern) {
    uint32_t random{0x544658};
    for(unsigned int index{0}; index != image.pixels.size(); ++index) {
      random = random * 1664525u + 1013904223u;
      image.pixels[index] = pattern == 0 ? 255 : pattern == 1 ? static_cast<uint8_t>(index) : pattern == 2 ? static_cast<uint8_t>(index / 320) : static_cast<uint8_t>(random >> 24);
    }
    auto const encoded{tfx::render::encode_ilbm(image,colours)};
    require(decode(encoded).pixels == image.pixels,"ILBM bitplane or compression round trip changed pixels");
  }
  // Odd widths exercise padded bitplane rows; wide images require multiple
  // ByteRun packets per row, including both literal and repeated packets.
  for(auto const &size : {vec2<int>{1001,603},vec2<int>{2049,3}}) {
    image.resize(size);
    for(unsigned int pattern{0}; pattern != 2; ++pattern) {
      uint32_t random{0x544658};
      for(auto &pixel : image.pixels) {
        random = random * 1664525u + 1013904223u;
        pixel = pattern ? static_cast<uint8_t>(random >> 24) : 255;
      }
      auto const restored{decode(tfx::render::encode_ilbm(image,colours))};
      require(restored.size == image.size && restored.pixels == image.pixels,"high-resolution ILBM round trip changed dimensions or pixels");
    }
  }
  std::cout << "ILBM capture: square pixel aspect, palette, compressed row boundaries and complete indexed-image round trips passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
