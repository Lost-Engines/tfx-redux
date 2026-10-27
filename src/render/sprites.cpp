#include "sprites.h"
#include "cockpit/vector_hud.h"
#include <algorithm>

namespace tfx::render {

void draw_sprite(framebuffer &target, resources::indexed_image const &image, vec2<int> const &centre, framebuffer *const coverage, raster_clip const &clip) {
  if(target.hud_capture) { target.hud_capture->sprite(image,vec2d{centre},clip);return; }
  // Native 0xc0ef8 supplies a sprite centre; palette index zero is transparent.
  int const left{centre.x - static_cast<int>(image.size.x / 2)};
  int const top{centre.y - static_cast<int>(image.size.y / 2)};
  // Native 0xc0f95 clips in four-pixel VGA groups and preserves the starting plane phase.
  int const width{static_cast<int>(image.size.x)};
  int clipped_right{left + width};
  if(left >= clip.sprite_minimum.x) {
    if(clipped_right >= clip.sprite_limit.x) clipped_right = (left / 4 + width / 4 - (clipped_right - clip.sprite_limit.x) / 4) * 4;
  } else {
    int const skipped_groups{(clip.sprite_minimum.x - left) / 4};
    int const remaining_width{width - skipped_groups * 4};
    if(clip.sprite_minimum.x + remaining_width > clip.sprite_limit.x) clipped_right = (clip.sprite_minimum.x / 4 + remaining_width / 4 - (clip.sprite_minimum.x + remaining_width - clip.sprite_limit.x) / 4) * 4;
  }
  for(int y{std::max({0,top,clip.sprite_minimum.y})}; y < std::min({screen_height,top + static_cast<int>(image.size.y),clip.sprite_limit.y}); ++y) {
    for(int x{std::max({0,left,clip.sprite_minimum.x})}; x < std::min(screen_width,clipped_right); ++x) {
      unsigned int const source_x{static_cast<unsigned int>(x - left)}, source_y{static_cast<unsigned int>(y - top)};
      unsigned int source{source_y * image.size.x + source_x};
      if(source_x >= image.size.x) {
        // Unrounded initial limits can expose up to three bytes in the next stored plane.
        // Reconstruct that planar read instead of reading past the decoded image.
        unsigned int const plane_bytes{static_cast<unsigned int>(image.pixels.size() / 4)};
        unsigned int const address{(source_x % 4) * plane_bytes + source_y * (image.size.x / 4) + source_x / 4};
        source = (address % plane_bytes) * 4 + address / plane_bytes;
      }
      auto const pixel{image.pixels.at(source)};
      if(!pixel) continue;
      auto const destination{static_cast<unsigned int>(y * screen_width + x)};
      target.write(destination,pixel);
      if(coverage) coverage->pixels[destination] = 255;
    }
  }
}
void draw_monochrome(framebuffer &target, resources::monochrome_image const &image, vec2<int> const &position, uint8_t const colour, framebuffer *const coverage, raster_clip const &clip) {
  // Native 0xcda48 uses a top-left origin and MSB-first packed rows. A set bit may paint black.
  // Native arguments are sign-extended words, including callers that supplied MOVZX values.
  vec2<int> const origin{math::signed_word(static_cast<uint16_t>(position.x)),math::signed_word(static_cast<uint16_t>(position.y))};
  if(target.hud_capture) { target.hud_capture->mask(image,origin,clip);return; }
  unsigned int const stride{(image.size.x + 7) / 8};
  for(int y{std::max({0,origin.y,clip.glyph_minimum.y})}; y < std::min({screen_height,origin.y + static_cast<int>(image.size.y),clip.glyph_limit.y}); ++y) {
    for(int x{std::max({0,origin.x,clip.glyph_minimum.x})}; x < std::min({screen_width,origin.x + static_cast<int>(image.size.x),clip.glyph_limit.x}); ++x) {
      unsigned int const column{static_cast<unsigned int>(x - origin.x)}, row{static_cast<unsigned int>(y - origin.y)};
      if(!(image.rows.at(row * stride + column / 8) & (128u >> (column % 8)))) continue;
      auto const destination{static_cast<unsigned int>(y * screen_width + x)};
      target.write(destination,colour);
      if(coverage) coverage->pixels[destination] = 255;
    }
  }
}

} // namespace tfx::render
