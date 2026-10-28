#pragma once

#include <algorithm>
#include <span>
#include "render/framebuffer.h"

namespace tfx::render {

// Half-open atlas region mapped straight to a source canvas. Sloping sides
// retain the authored screen bevel without first resizing its pixel artwork.
struct texture_patch {
  vec2d minimum, maximum;
  vec2d source, extent;
  double left_shift{0}, right_shift{0};
  double inverse_height, horizontal_scale, vertical_scale;

  texture_patch(vec2d const &minimum, vec2d const &maximum, vec2d const &source,
      vec2d const &extent, double left_shift=0, double right_shift=0)
    : minimum{minimum}, maximum{maximum}, source{source}, extent{extent},
      left_shift{left_shift}, right_shift{right_shift},
      inverse_height{1/(maximum.y-minimum.y)}, horizontal_scale{extent.x/(maximum.x-minimum.x)}, vertical_scale{extent.y/(maximum.y-minimum.y)} {}
};

struct texture_overlay {
  // Legacy atlas-sized overrides: zero keeps the art, 0x100|index replaces it.
  std::span<uint16_t const> texels{};
  std::span<texture_patch const> patches{};
  std::span<uint8_t const> cells{}; // One-based patch IDs, indexed like the atlas.
  std::span<uint8_t const> source{};
  vec2<int> source_size{320,200};

  bool empty() const { return texels.empty() && patches.empty(); }
  uint16_t sample(unsigned int const at, double const u, double const v) const {
    if(!patches.empty() && cells[at]) {
      auto const &patch{patches[cells[at]-1u]};
      if(v>=patch.minimum.y && v<patch.maximum.y) {
        double left{patch.minimum.x}, right{patch.maximum.x}, scale{patch.horizontal_scale};
        if(patch.left_shift || patch.right_shift) {
          double const y{(v-patch.minimum.y)*patch.inverse_height};
          left+=patch.left_shift*y;right+=patch.right_shift*y;
          scale=patch.extent.x/(right-left);
        }
        if(u>=left && u<right) {
          int const sx{std::clamp(static_cast<int>(patch.source.x+(u-left)*scale),0,source_size.x-1)};
          int const sy{std::clamp(static_cast<int>(patch.source.y+(v-patch.minimum.y)*patch.vertical_scale),0,source_size.y-1)};
          return static_cast<uint16_t>(0x100u|source[static_cast<size_t>(sy*source_size.x+sx)]);
        }
      }
    }
    return texels.empty() ? 0 : texels[at];
  }
};

} // namespace tfx::render
