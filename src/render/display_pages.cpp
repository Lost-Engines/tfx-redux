#include "display_pages.h"

namespace tfx::render {

void display_pages::reset() {
  for(auto &page : *pages) page.pixels.fill(0);
  current = first_page;
  previous_offset_y = -1;
}
void display_pages::begin_frame(framebuffer &target) const {
  target = (*pages)[current];
}
void display_pages::finish_frame(framebuffer const &target) {
  (*pages)[current] = target;
  current = (current + 1) % display_page_count;
}
bool display_pages::refresh_panel(uint8_t &remaining, int16_t const offset_y) {
  if(offset_y != previous_offset_y) {
    remaining = display_page_count;
    previous_offset_y = offset_y;
  }
  if(!remaining) return false;
  --remaining;
  return true;
}

} // namespace tfx::render
