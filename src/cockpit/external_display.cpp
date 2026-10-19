#include "external_display.h"
#include "cockpit/format.h"

namespace tfx::cockpit {

external_display::external_display(instrument_assets const &source, presentation::fonts const &source_fonts) : assets{source}, fonts{source_fonts} {}
void external_display::draw(render::software_renderer &renderer, external_view_state const &state, render::framebuffer *const coverage) const {
  if(!state.enabled) return;
  int constexpr panel_top{189}, panel_bottom{199}, label_row{191};
  // Native 0x80794 first ANDs indices with 15, then ORs with 128. Retain
  // the low palette bits of the scenery behind this small status strip.
  for(int y{panel_top}; y <= panel_bottom; ++y) for(int x{0}; x != render::screen_width; ++x) {
    auto const pixel{static_cast<unsigned int>(y * render::screen_width + x)};
    renderer.output.write(pixel,static_cast<uint8_t>((renderer.output.pixels[pixel] & 15) | 128));
    if(coverage) coverage->pixels[pixel] = 255;
  }
  auto const paint{[&](int const x, int const y, uint8_t const colour) { renderer.output.write(static_cast<unsigned int>(y * render::screen_width + x),colour); }};
  for(int x{0}; x != render::screen_width; ++x) { paint(x,panel_top,2);paint(x,panel_bottom,5); }
  for(int y{panel_top + 1}; y != panel_bottom; ++y) { paint(0,y,2);paint(render::screen_width - 1,y,5); }
  auto const text{[&](int const x, std::string const &message) {
    presentation::font_id constexpr fixed_font{1};
    fonts.draw(renderer.output,fixed_font,{x + 1,label_row + 1},message,143,coverage);
    fonts.draw(renderer.output,fixed_font,{x,label_row},message,15,coverage);
  }};
  text(5,detail::decimal(assets.external_number_formats[0],static_cast<double>(-static_cast<long double>(state.height) * assets.external_altitude_scale),0));
  text(130,detail::decimal(assets.external_number_formats[1],static_cast<double>(static_cast<long double>(state.speed) * assets.speed_scale),0));
  text(248,detail::formatted(assets.external_number_formats[2],static_cast<unsigned long>((static_cast<unsigned int>(state.heading) * 360) >> 16)));
}

} // namespace tfx::cockpit
