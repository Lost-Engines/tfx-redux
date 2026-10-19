#include "score_player.h"
#include <algorithm>

namespace tfx::presentation {

score_player::score_player(fonts const &fonts, score_assets const &assets, score_table entries,
  std::optional<unsigned int> const selected, std::span<uint8_t const> const background) :
  font_data{fonts}, assets{assets}, entries{std::move(entries)}, selected{selected}, background{resources::decode_screen(background)} {
  display = this->background.image;
}
void score_player::draw() {
  render::framebuffer text;
  render::framebuffer coverage;
  for(unsigned int row{0}; row != entries.size(); ++row) {
    auto const &entry{entries[row]};
    auto const number{std::to_string(entry.score)};
    auto const name{entry.firstname + " -" + entry.callsign + "- " + entry.surname};
    int const y{61 + static_cast<int>(row) * 9};
    auto const item{[&](std::string const &label, int const x) {
      if(selected == row) font_data.draw(text,6,{x + 1,y + 1},label,0,&coverage);
      uint8_t const colour{static_cast<uint8_t>(selected == row ? 144 + assets.highlight[pulse] : 55)};
      font_data.draw(text,6,{x,y},label,colour,&coverage);
    }};
    item(number,268 - font_data.measure(6,number));
    item(name,45);
  }
  display = background.image;
  for(int y{0}; y != render::screen_height; ++y) for(int x{0}; x != render::screen_width; ++x) {
    unsigned int const offset{static_cast<unsigned int>(y * render::screen_width + x)};
    // Native 0xa6c3c expands its text clip rectangle for 46 retraces, then pulses the newly ranked pilot.
    int const width{static_cast<int>(std::min(reveal,45u))};
    if(x >= 160 - width * 3 && x < 160 + width * 3 && y >= 104 - width && y <= 104 + width && coverage.pixels[offset]) display.write(offset,text.pixels[offset]);
  }
}
void score_player::press(uint8_t const scan) {
  if(reveal == 46 && scan) finished = true;
}
bool score_player::advance(uint8_t const mouse_buttons) {
  if(finished) return true;
  if(reveal < 46) {
    draw();
    ++reveal;
  } else {
    if(mouse_buttons) { finished = true;return true; }
    draw();
    if(selected) pulse = (pulse + 1) % static_cast<unsigned int>(assets.highlight.size());
  }
  return false;
}
render::framebuffer const &score_player::image() const {
  return display;
}
render::palette const &score_player::colours() const {
  return background.colours;
}

} // namespace tfx::presentation
