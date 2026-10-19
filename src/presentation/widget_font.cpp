#include "widget_font.h"
#include <bit>
#include <stdexcept>

namespace tfx::presentation {

widget_font::widget_font(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr code{0x70aa4}, advances{code + 0x60f49}, mapping{code + 0x6103d}, rows{code + 0x61132}, glyphs_per_row{115};
  for(unsigned int letter{0}; letter != characters.size(); ++letter) {
    auto &glyph{characters[letter]};
    glyph.advance = static_cast<uint8_t>(source.byte(advances + letter) + 1);
    unsigned int const index{source.byte(mapping + letter)};
    if(index >= glyphs_per_row) throw std::runtime_error{"widget glyph index exceeds its original row bank"};
    for(unsigned int row{0}; row != glyph.rows.size(); ++row) glyph.rows[row] = source.byte(rows + row * glyphs_per_row + index);
  }
}
int32_t widget_font::measure(std::string_view const text) const {
  uint32_t width{0};
  for(uint8_t const letter : text) {
    if(!letter) break;
    width += characters[letter].advance;
  }
  return std::bit_cast<int32_t>(width);
}
void widget_font::draw(render::framebuffer &target, vec2<int> const &position, std::string_view const text,
  uint8_t const colour, render::framebuffer *const coverage) const {
  int64_t x{position.x};
  // Native 0xd172b clamps the starting row after subtracting two; it does not skip the glyph's accent rows.
  int64_t const top{position.y < 2 ? 0 : position.y - 2};
  for(uint8_t const letter : text) {
    if(!letter) break;
    auto const &glyph{characters[letter]};
    for(unsigned int row{0}; row != glyph.rows.size(); ++row) for(unsigned int column{0}; column != 8; ++column) {
      int64_t const px{x + column}, py{top + row};
      if((glyph.rows[row] & (1u << column)) && px >= 0 && px < render::screen_width && py >= 0 && py < render::screen_height) {
        unsigned int const offset{static_cast<unsigned int>(py * render::screen_width + px)};
        target.write(offset,colour);
        if(coverage) coverage->pixels[offset] = 255;
      }
    }
    x += glyph.advance;
  }
}

} // namespace tfx::presentation
