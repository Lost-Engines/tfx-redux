#pragma once

#include <array>
#include <string_view>
#include "render/framebuffer.h"
#include "resources/binary.h"

namespace tfx::presentation {

using font_id = uint8_t;
unsigned int constexpr font_count{10};
struct font_glyph {
  vec2<unsigned int> size{0, 0};
  vec2<int> offset{0, 0};
  unsigned int advance{0};
  bool available{true};
  resources::bytes mask{};
};
class fonts {
private:
  struct font {
    unsigned int line_height{0};
    unsigned int width_trim{0};
    std::array<unsigned int, 256> widths{};
    std::array<font_glyph, 256> glyphs{};
  };
  std::array<font, font_count> styles;
  unsigned int static constexpr driver_columns{8};
  unsigned int static constexpr driver_rows{6};
  unsigned int static constexpr driver_characters{64};
  unsigned int static constexpr driver_advance{9};
  std::array<std::array<uint8_t, driver_characters>, driver_rows> driver;

public:
  explicit fonts(std::span<uint8_t const> executable);
  font_glyph const &glyph(font_id style, unsigned char character) const;
  unsigned int line_height(font_id style) const;
  int32_t measure(font_id style, std::string_view text) const;
  void draw(render::framebuffer &target, font_id style, vec2<int> const &position, std::string_view text, uint8_t colour, render::framebuffer *coverage = nullptr) const;
  void draw_driver(render::framebuffer &target, vec2<int> const &position, std::string_view text, uint8_t colour, render::framebuffer *coverage = nullptr) const;
};

} // namespace tfx::presentation
