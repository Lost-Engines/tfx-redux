#include "fonts.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace tfx::presentation {
namespace {

unsigned int constexpr data_file{973476}, code_file{0x70aa4}, character_map{0xcc2f};
struct proportional_layout {
  unsigned int pointers;
  unsigned int widths;
  unsigned int spacing;
  unsigned int space;
  bool uppercase;
};
std::array<proportional_layout,font_count> constexpr layouts{{
  {
    .pointers{0xd23c},
    .widths{0xcd5b},
    .spacing{2},
    .space{6},
    .uppercase{false},
  },
  {},{},
  {
    .pointers{0xcdc0},
    .widths{0xcc6f},
    .spacing{1},
    .space{4},
    .uppercase{true},
  },
  {
    .pointers{0xce8c},
    .widths{0xcca2},
    .spacing{2},
    .space{6},
    .uppercase{true},
  },
  {
    .pointers{0xd024},
    .widths{0xccd5},
    .spacing{1},
    .space{5},
    .uppercase{true},
  },
  {
    .pointers{0xcf58},
    .widths{0xcd8b},
    .spacing{1},
    .space{3},
    .uppercase{false},
  },
  {
    .pointers{0xd08c},
    .widths{0xccef},
    .spacing{2},
    .space{17},
    .uppercase{false},
  },
  {
    .pointers{0xd11c},
    .widths{0xcd13},
    .spacing{1},
    .space{5},
    .uppercase{false},
  },
  {
    .pointers{0xd1ac},
    .widths{0xcd37},
    .spacing{1},
    .space{5},
    .uppercase{false},
  },
}};

} // anonymous namespace

fonts::fonts(std::span<uint8_t const> const executable) {
  resources::binary_view const view{executable};
  // C1378's fixed driver font is distinct from the ten presentation styles.
  for(unsigned int row{0}; row != driver_rows; ++row) for(unsigned int code{0}; code != driver_characters; ++code)
    driver[row][code] = view.byte(code_file + 0x509e5 + row * driver_characters + code);
  std::array<unsigned int,font_count> constexpr line_heights{13,9,6,16,12,7,7,18,9,7}, trims{2,0,0,0,0,0,0,2,1,1};
  for(unsigned int style{0}; style != font_count; ++style) {
    auto &font{styles[style]};
    font.line_height = line_heights[style];
    font.width_trim = trims[style];
    auto const &layout{layouts[style]};
    for(unsigned int code{0}; code != font.glyphs.size(); ++code) {
      auto &glyph{font.glyphs[code]};
      auto &width{font.widths[code]};
      if(style == 1 || style == 2) {
        // The two fixed fonts store one 64-character bit-mask lookup per row in the code object.
        unsigned int const columns{style == 1 ? 8u : 6u}, rows{style == 1 ? 7u : 6u}, offset{style == 1 ? 0x5bb51u : 0x63f02u};
        width = columns;
        if(!(code & 63)) continue;
        glyph.size = vec2<unsigned int>{columns,rows};
        glyph.advance = columns;
        for(unsigned int y{0}; y != rows; ++y) {
          auto const bits{view.byte(code_file + offset + y * 64 + (code & 63))};
          for(unsigned int x{0}; x != columns; ++x) glyph.mask.push_back(bits & (1 << x) ? 255 : 0);
        }
        continue;
      }
      if(code == ' ') { width = layout.space;glyph.advance = layout.space;continue; }
      bool const extended{style == 6 && code > 127};
      int const metric_index{std::bit_cast<int8_t>(view.byte(data_file + character_map + (code & 63)))};
      // Width measurement and drawing are separate native routines: their case handling and final spacing can differ.
      if(layout.uppercase && (code & 63) == 32) width = layout.space;
      else if(extended) width = view.byte(data_file + 0xd484 + code - 128);
      else if(metric_index != -1 || (style != 3 && style != 4 && style != 5)) width = view.byte(data_file + layout.widths + metric_index) + layout.spacing;
      unsigned int const drawn_code{layout.uppercase && code >= 'a' && code <= 'z' ? code - ('a' - 'A') : code};
      int const index{extended ? static_cast<int>(code - 128) : std::bit_cast<int8_t>(view.byte(data_file + character_map + (drawn_code & 63)))};
      if(index == -1) continue;
      unsigned int const advance{view.byte(data_file + (extended ? 0xd484 : layout.widths) + index)};
      if(extended && !advance) continue;
      glyph.advance = advance + layout.spacing;
      auto const pointer{view.le32(data_file + (extended ? 0xd2fc : layout.pointers) + static_cast<unsigned int>(index) * 4)};
      if(!pointer) { glyph.available = false;continue; }
      if(pointer > executable.size() - data_file - 2) { glyph.available = false;continue; }
      unsigned int const offset{data_file + pointer};
      glyph.size = vec2<unsigned int>{view.byte(offset),view.byte(offset + 1)};
      if(extended) glyph.offset.y = -2;
      unsigned int const row_bytes{(glyph.size.x + 7) / 8};
      auto const bits{view.slice(offset + 2,row_bytes * glyph.size.y)};
      for(unsigned int y{0}; y != glyph.size.y; ++y) for(unsigned int x{0}; x != glyph.size.x; ++x) glyph.mask.push_back(bits[y * row_bytes + x / 8] & (128 >> (x % 8)) ? 255 : 0);
    }
  }
}

font_glyph const &fonts::glyph(font_id const style, unsigned char const character) const { return styles.at(style).glyphs[character]; }
unsigned int fonts::line_height(font_id const style) const { return styles.at(style).line_height; }
int32_t fonts::measure(font_id const style, std::string_view const text) const {
  auto const &font{styles.at(style)};
  uint32_t width{0};
  for(unsigned char const character : text) { if(!character) break;width += font.widths[character]; }
  return std::bit_cast<int32_t>(width - font.width_trim);
}
void fonts::draw(render::framebuffer &target, font_id const style, vec2<int> const &position, std::string_view const text, uint8_t const colour, render::framebuffer *const coverage) const {
  int x{position.x};
  bool first{true};
  for(unsigned char const character : text) {
    if(!character || ((style == 1 || style == 2) && !(character & 63))) break;
    auto const &glyph{this->glyph(style,character)};
    if(!glyph.available) throw std::runtime_error{"original presentation font has no glyph for this character"};
    for(unsigned int row{0}; row != glyph.size.y; ++row) for(unsigned int column{0}; column != glyph.size.x; ++column) {
      if(!glyph.mask[row * glyph.size.x + column]) continue;
      int const px{x + glyph.offset.x + static_cast<int>(column)}, py{position.y + glyph.offset.y + static_cast<int>(row)};
      if(px < 0 || px >= render::screen_width || py < 0 || py >= render::screen_height) continue;
      auto const index{static_cast<unsigned int>(py * render::screen_width + px)};
      target.write(index,colour);
      if(coverage) coverage->pixels[index] = 255;
    }
    // The fixed eight-pixel renderer special-cases a following uppercase I as a four-pixel cell.
    x += style == 1 && character == 'I' && !first ? 4 : static_cast<int>(glyph.advance);
    first = false;
  }
}

void fonts::draw_driver(render::framebuffer &target, vec2<int> const &position, std::string_view const text, uint8_t const colour, render::framebuffer *const coverage) const {
  int x{position.x};
  for(unsigned char const character : text) {
    unsigned int const code{character & (driver_characters - 1)};
    if(!code) break;
    for(unsigned int row{0}; row != driver_rows; ++row) for(unsigned int column{0}; column != driver_columns; ++column) {
      if(!(driver[row][code] & (1u << column))) continue;
      int const px{x + static_cast<int>(column)}, py{position.y + static_cast<int>(row)};
      if(px < 0 || px >= render::screen_width || py < 0 || py >= render::screen_height) continue;
      auto const index{static_cast<unsigned int>(py * render::screen_width + px)};
      target.write(index,colour);
      if(coverage) coverage->pixels[index] = 255;
    }
    x += static_cast<int>(driver_advance);
  }
}

} // namespace tfx::presentation
