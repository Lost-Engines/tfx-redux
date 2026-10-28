#pragma once

#include <array>
#include <string_view>
#include "cockpit/instrument_assets.h"
#include "models/shape.h"
#include "render/texture_overlay.h"

namespace tfx::cockpit {

// The six front-canopy models share two atlases. Map their instrument regions
// directly to the complete F1 panel, preserving its native pixels and updates.
class virtual_displays {
  std::array<std::vector<uint8_t>,2> cells;
  std::array<std::vector<render::texture_patch>,2> patches;
  std::array<uint8_t,320*200> panel{};
  void add(unsigned int page, render::texture_patch const &patch);
public:
  explicit virtual_displays(instrument_assets const &instruments);
  void update(render::framebuffer const &frame);
  render::texture_overlay texture(std::string_view model, models::texture_binding const &binding) const;
};

} // namespace tfx::cockpit
