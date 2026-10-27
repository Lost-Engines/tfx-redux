#include "markers.h"
#include "cockpit/vector_hud.h"

namespace tfx::render {

void draw_marker(framebuffer &target, vec2<uint16_t> const &position, uint8_t const colour, marker_kind const kind, framebuffer *const coverage) {
  // Native C7030 uses a signed word shift for X; CD889's 2x2 points use a dword shift.
  // Callers establish a valid scanline. Horizontal overflow follows the linear VGA address.
  if(position.y >= screen_height || (kind == marker_kind::pixel && position.x >= 32768)) return;
  if(target.hud_capture) { target.hud_capture->pixel(vec2d{position});return; }
  unsigned int const size{kind == marker_kind::block ? 2u : 1u};
  for(unsigned int row{0}; row != size; ++row) for(unsigned int column{0}; column != size; ++column) {
    unsigned int const destination{(position.y + row) * screen_width + position.x + column};
    if(destination >= target.pixels.size()) continue;
    target.write(destination,colour);
    if(coverage) coverage->pixels[destination] = 255;
  }
}

} // namespace tfx::render
