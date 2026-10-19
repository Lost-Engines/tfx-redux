#pragma once

#include "presentation/arming_preview.h"
#include "presentation/arming_widgets.h"
#include "resources/media.h"

namespace tfx::presentation {

class arming_screen {
private:
  weapons::armament const &inventory;
  resources::executable_tables const &tables;
  flight::airframe const &profile;
  resources::screen_image background;
  dac_palette palette;
  arming_controller controls;
  arming_preview preview;
  render::software_renderer renderer, detail_renderer;
  render::framebuffer composed;
  vec2<int> resolution;
  double previous_drag{0};
  unsigned int fade_level{1};
  bool entering{true};
  bool leaving{false};
  uint8_t key{0};
  void draw(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t buttons);

public:
  arming_screen(resources::archive const &game, resources::executable_tables const &tables, weapons::armament &inventory,
    flight::player_aircraft aircraft, unsigned int default_fit, arming_entry entry = arming_entry::normal, vec2<int> const &resolution = {render::screen_width,render::screen_height});
  void press(uint8_t scan);
  bool rotating() const;
  bool cancelled() const;
  bool advance(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t buttons);
  render::framebuffer const &image() const;
  render::palette colours() const;
};

} // namespace tfx::presentation
