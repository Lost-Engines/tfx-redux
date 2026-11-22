#pragma once

#include <string_view>
#include "render/framebuffer.h"
#include "resources/binary.h"

namespace tfx::presentation {

// The proportional eight-row font used by native dialog widgets and planner labels.
class widget_font {
private:
  struct glyph {
    uint8_t advance;
    std::array<uint8_t, 8> rows;
  };
  std::array<glyph, 256> characters;

public:
  explicit widget_font(std::span<uint8_t const> executable);
  int32_t measure(std::string_view text) const;
  void draw(render::framebuffer &target, vec2<int> const &position, std::string_view text,
    uint8_t colour, render::framebuffer *coverage = nullptr) const;
};

} // namespace tfx::presentation
