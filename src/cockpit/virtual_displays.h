#pragma once

#include <array>
#include <string_view>
#include "cockpit/instrument_assets.h"
#include "models/shape.h"

namespace tfx::cockpit {

// The six front-canopy models share the two panel atlases. Values with bit
// 8 set replace a texel with an opaque, unlit display colour (including black).
// Zero leaves the original texture/lighting/transparency alone.
class virtual_displays {
  std::array<std::vector<uint16_t>,2> pages;
  instrument_assets const &instruments;
public:
  explicit virtual_displays(instrument_assets const &instruments);
  void update(render::framebuffer const &frame);
  std::span<uint16_t const> texture(std::string_view model, models::texture_binding const &binding) const;
};

} // namespace tfx::cockpit
