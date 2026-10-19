#include "drawing.h"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace tfx::presentation {

// Native widgets use inclusive endpoints and the same Bresenham tie rule as VGA lines.
void draw_line(render::framebuffer &image, vec2<int> from, vec2<int> const &to, uint8_t const colour) {
  if(from.x > to.x) { draw_line(image,vec2<int>{to},from,colour);return; }
  int const dx{to.x - from.x}, dy{std::abs(to.y - from.y)}, direction{from.y <= to.y ? 1 : -1};
  auto const pixel{[&] {
    if(from.x >= 0 && from.x < render::screen_width && from.y >= 0 && from.y < render::screen_height) image.write(from.y * render::screen_width + from.x,colour);
  }};
  int error{std::max(dx,dy) / 2};
  for(int step{0}; step <= std::max(dx,dy); ++step) {
    pixel();
    if(dx > dy) {
      ++from.x;
      error -= dy;
      if(error < 0) { from.y += direction;error += dx; }
    } else {
      from.y += direction;
      error -= dx;
      if(error < 0) { ++from.x;error += dy; }
    }
  }
}

bool clip_line(vec2<int> &from, vec2<int> &to,
  vec2<int> const &minimum, vec2<int> const &maximum) {
  // Clip X before Y. Native 0xbf53d measures from the farther endpoint;
  // choosing the nearer one changes the truncating division at boundaries.
  auto const move{[](int &value, int &other, int const target, int const end, int const end_other) {
    int const distance{std::abs(target - value)}, remainder{std::abs(end - target)}, total{std::abs(end - value)};
    if(total) other = distance > remainder ? other + (end_other - other) * distance / total : end_other - (end_other - other) * remainder / total;
    value = target;
  }};
  for(unsigned int axis{0}; axis != 2; ++axis) {
    if(std::max(from[axis],to[axis]) < minimum[axis] || std::min(from[axis],to[axis]) > maximum[axis]) return false;
    if(from[axis] < minimum[axis]) move(from[axis],from[1 - axis],minimum[axis],to[axis],to[1 - axis]);
    if(to[axis] < minimum[axis]) move(to[axis],to[1 - axis],minimum[axis],from[axis],from[1 - axis]);
    if(from[axis] > maximum[axis]) move(from[axis],from[1 - axis],maximum[axis],to[axis],to[1 - axis]);
    if(to[axis] > maximum[axis]) move(to[axis],to[1 - axis],maximum[axis],from[axis],from[1 - axis]);
  }
  if(from.x < minimum.x || from.x > maximum.x || to.x < minimum.x || to.x > maximum.x) return false;
  return true;
}
void draw_clipped_line(render::framebuffer &image, vec2<int> from, vec2<int> to,
  vec2<int> const &minimum, vec2<int> const &maximum, uint8_t const colour) {
  if(clip_line(from,to,minimum,maximum)) draw_line(image,vec2<int>{from},to,colour);
}
void draw_patterned_line(render::framebuffer &image, vec2<int> from, vec2<int> to,
  vec2<int> const &minimum, vec2<int> const &maximum, uint8_t const colour, uint8_t pattern) {
  if(!clip_line(from,to,minimum,maximum)) return;
  // Native 0xdf790 starts from the lesser X (swapping equal X too). Its
  // pattern phase comes from the VGA plane mask, not the screen X directly.
  if(from.x >= to.x) { auto const saved{vec2<int>{from}};from = to;to = saved; }
  std::array<unsigned int,4> constexpr phases{1,2,4,0};
  auto const rotate{[&](unsigned int const bits) { pattern = static_cast<uint8_t>((pattern << bits) | (pattern >> (8 - bits))); }};
  rotate(phases[static_cast<unsigned int>(from.x) & 3]);
  int const dx{to.x - from.x}, dy{std::abs(to.y - from.y)}, direction{from.y <= to.y ? 1 : -1};
  int error{std::max(dx,dy) / 2};
  for(int step{0}; step <= std::max(dx,dy); ++step) {
    // The X-major loop paints its final endpoint regardless of the pattern.
    if((pattern & 128) || (dx > dy && step == dx)) image.write(from.y * render::screen_width + from.x,colour);
    rotate(1);
    if(dx > dy) { ++from.x;error -= dy;if(error < 0) { from.y += direction;error += dx; } }
    else { from.y += direction;error -= dx;if(error < 0) { ++from.x;error += dy; } }
  }
}

unsigned int draw_partial_line(render::framebuffer &image, vec2<int> from, vec2<int> to,
  vec2<int> const &minimum, vec2<int> const &maximum, uint8_t const colour, unsigned int const extent) {
  if(!clip_line(from,to,minimum,maximum)) return 0;
  // Native 0xe15f0 reveals the line with a signed 16.16 DDA, rather than the
  // Bresenham painter used for completed segments. The last endpoint is excluded.
  unsigned int const axis{std::abs(to.x - from.x) > std::abs(to.y - from.y) ? 0u : 1u};
  unsigned int const length{static_cast<unsigned int>(std::abs(to[axis] - from[axis]))};
  if(!length) return 0;
  unsigned int const visible{std::min(extent,length)};
  int const slope{(to[1 - axis] - from[1 - axis]) * 65536 / static_cast<int>(length)};
  int minor{from[1 - axis] * 65536 + (slope >> 1)};
  int const direction{to[axis] >= from[axis] ? 1 : -1};
  for(unsigned int step{0}; step != visible; ++step) {
    from[1 - axis] = minor >> 16;
    image.write(from.y * render::screen_width + from.x,colour);
    from[axis] += direction;
    minor += slope;
  }
  return length - visible;
}

} // namespace tfx::presentation
