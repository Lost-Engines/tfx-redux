#pragma once

#include "presentation/cursor.h"
#include "presentation/menu.h"
#include "presentation/timing.h"

namespace tfx::presentation {

class menu_player {
private:
  menu_program source;
  variables &state;
  pointer_style const &pointer;
  std::optional<resources::animation> movie;
  std::optional<animation_clock> clock;
  render::framebuffer background;
  render::framebuffer display;
  dac_palette native_colours;
  dac_palette displayed_colours{};
  unsigned int fade_level{0};
  enum class phase : uint8_t {
    entering,
    choosing,
    releasing,
    leaving,
    complete
  } stage{phase::entering};
  std::optional<unsigned int> selected;
  void compose(vec2<int16_t> const &position, bool pointer_visible);

public:
  menu_player(menu_program source, std::span<uint8_t const> media, variables &state,
    std::array<pointer_style, 2> const &pointers, bios_counter counter);
  bool advance(bios_counter counter, vec2<int16_t> const &position, bool left_down, bool pointer_visible = true);
  render::framebuffer const &image() const;
  render::palette colours() const;
  std::optional<unsigned int> choice() const;
};

} // namespace tfx::presentation
