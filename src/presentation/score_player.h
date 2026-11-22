#pragma once

#include "presentation/fonts.h"
#include "presentation/scores.h"
#include "resources/media.h"

namespace tfx::presentation {

class score_player {
private:
  fonts const &font_data;
  score_assets const &assets;
  score_table entries;
  std::optional<unsigned int> selected;
  resources::screen_image background;
  render::framebuffer display;
  unsigned int reveal{0};
  unsigned int pulse{0};
  bool finished{false};
  void draw();

public:
  score_player(fonts const &fonts, score_assets const &assets, score_table entries,
    std::optional<unsigned int> selected, std::span<uint8_t const> background);
  void press(uint8_t scan);
  bool advance(uint8_t mouse_buttons);
  render::framebuffer const &image() const;
  render::palette const &colours() const;
};

} // namespace tfx::presentation
