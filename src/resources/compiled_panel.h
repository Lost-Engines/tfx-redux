#pragma once

#include <string>
#include <string_view>
#include "render/framebuffer.h"
#include "resources/binary.h"

namespace tfx::resources {

struct screen_overlay {
  render::framebuffer image;
  render::framebuffer coverage;
  void paint(render::framebuffer &target) const;
};

// The planner's panel was compiled into a fixed VGA painting stream in TFX.DAT.
// Decode that restricted artwork representation into indexed pixels, without executing game code.
screen_overlay decode_planner_panel(std::span<uint8_t const> executable);
class planner_artwork {
private:
  screen_overlay panel;
  std::array<uint8_t, 80 * 3> interface_colours;
  std::array<uint8_t, 16 * 3> marker_colours;
  std::array<std::string, 5> region_names;

public:
  explicit planner_artwork(std::span<uint8_t const> executable);
  screen_overlay const &backdrop() const;
  std::array<uint8_t, 256 * 3> colours(std::span<uint8_t const> map_colours) const;
  std::string const &region(std::string_view world) const;
};

} // namespace tfx::resources
