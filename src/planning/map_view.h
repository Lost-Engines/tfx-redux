#pragma once

#include "planning/document.h"
#include "presentation/widget_font.h"
#include "resources/archive.h"
#include "resources/compiled_panel.h"

namespace tfx::planning {

enum class map_layer : uint8_t {
  physical,
  awac,
  political
};
class map_view {
private:
  resources::planner_artwork const &artwork;
  presentation::widget_font const &font;
  document const &plan;
  struct layer {
    render::framebuffer image;
    std::array<uint8_t, 256 * 3> colours;
  };
  std::array<layer, 3> layers;
  map_layer selected{map_layer::physical};
  render::framebuffer display;

public:
  map_view(resources::archive const &game, resources::planner_artwork const &artwork,
    presentation::widget_font const &font, document const &plan);
  void select(map_layer layer);
  void route(render::framebuffer &target, unsigned int count) const;
  unsigned int extend(render::framebuffer &target, unsigned int count, unsigned int extent) const;
  void draw();
  render::framebuffer const &image() const;
  std::array<uint8_t, 256 * 3> const &palette() const;
};

} // namespace tfx::planning
