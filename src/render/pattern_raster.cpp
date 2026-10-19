#include "pattern_raster.h"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace tfx::render {

void pattern_pixel(framebuffer &output, int const x, int const y, uint8_t const colour, int const mode) {
  if(x < 0 || y < 0 || x >= output.size.x || y >= output.size.y) return;
  if(mode == 0 && ((x + y) & 1) == 0) return;
  if(mode == 1 && (x & 3) != (y & 1 ? 3 : 1)) return;
  if(mode == 7 && (y & 1)) return;
  auto &destination{output.pixels[static_cast<unsigned int>(y * output.size.x + x)]};
  if(mode == 10) destination ^= colour;
  else if(mode == 11) destination &= colour;
  else if(mode == 12) destination |= colour;
  else destination = colour;
  if(!output.coverage.empty()) output.coverage[static_cast<size_t>(y * output.size.x + x)] = 255;
}


void pattern_span(framebuffer &output, uint16_t &procedural_seed, int const y, double const left, double const right, uint8_t const colour, int const mode, int const bottom, bool const inclusive, bool const paint) {
  /// Preserve native VGA byte-group masks, including sparse dots beyond the nominal right edge
  int const first{std::max(0,static_cast<int>(std::ceil(left - .5)))}, last{std::min(output.size.x,inclusive ? static_cast<int>(std::floor(right)) + 1 : static_cast<int>(std::ceil(right - .5)))};
  int const first_byte{static_cast<int>(std::floor(left / 4))}, last_byte{static_cast<int>(std::floor(right / 4))};
  if(mode == 4) {
    for(int byte{first_byte}; byte <= last_byte; ++byte) {
      unsigned int const low{static_cast<unsigned int>(procedural_seed & 255)}, high{static_cast<unsigned int>(procedural_seed >> 8)};
      auto const sum{high + low}, next_high{sum & 255}, next_low{(low + next_high + (sum > 255)) & 255};
      procedural_seed = static_cast<uint16_t>((next_high << 8) | next_low);
      auto const mask{next_low & (((bottom - y) & 1) ? 5u : 10u)};
      if(!paint) continue;
      for(unsigned int bit{0}; bit != 4; ++bit) {
        int const x{byte * 4 + static_cast<int>(bit)};
        if(x >= first && x < last && (mask & (1u << bit))) pattern_pixel(output,x,y,colour,-1);
      }
    }
    return;
  }
  if(mode == 2) {
    if(y & 1) return;
    int const phase{(first_byte + (((bottom & ~1) - y) / 2) + (bottom & 1)) & 1};
    int const end{(first_byte + ((last_byte - first_byte) & ~1)) * 4 + 3};
    for(int x{first_byte * 4}; x <= end; ++x) if((x & 7) == phase * 4 + 3) pattern_pixel(output,x,y,colour,-1);
    return;
  }
  if(mode == 3) {
    int const row{bottom - y};
    if(row % 3) return;
    int const offset{(bottom & 1 ? 0 : 2) + ((row / 3) & 1) * 2};
    int const start{(first_byte + (offset & 3)) * 4 + 3};
    int const end{start + static_cast<int>(std::floor((last_byte - first_byte - 1) / 4.0)) * 16};
    for(int x{start}; x <= end; x += 16) pattern_pixel(output,x,y,colour,-1);
    return;
  }
  for(int x{first}; x < last; ++x) pattern_pixel(output,x,y,colour,mode);
}


void pattern_polygon(framebuffer &output, uint16_t &seed, std::span<polygon_span const> const spans, uint8_t const colour, int const mode) {
  if(spans.empty()) return;
  if(mode == 7) {
    bool narrow{false};
    for(auto const &span : spans | std::views::reverse) {
      if(span.row & 1) {
        if(span.left > span.right) return;
        continue;
      }
      // Original E4CCF shifts the next left endpoint by one after a single-
      // byte span, rather than its usual two. Retain this authored-era quirk:
      // it can terminate the rest of a tapered line-stippled face.
      int const first{span.left >> (narrow ? 1 : 2)},last{span.right >> 2};
      if(first > last) return;
      for(int byte{first}; byte <= last; ++byte) for(int plane{0}; plane != 4; ++plane) {
        if((byte == first && plane < (span.left & 3)) || (byte == last && plane > (span.right & 3))) continue;
        int const at{span.row * screen_width + byte * 4 + plane};
        if(at >= 0 && at < screen_width * screen_height) output.write(static_cast<unsigned int>(at),colour);
      }
      narrow = first == last;
    }
    return;
  }
  if(mode == 2 || mode == 3) {
    // These sparse VGA painters skip whole scanlines and alternate byte-group
    // offsets. A span narrower than one VGA byte follows their special skip
    // branch, so its phase cannot be inferred from y alone.
    int const stride{mode == 2 ? 2 : 3};
    int index{static_cast<int>(spans.size()) - 1};
    bool offset{mode == 2 ? (spans.back().row & 1) != 0 : (spans.back().row & 1) == 0};
    if(mode == 2) {
      // Its initial row and the first selected row have explicit pixel-width
      // tests; later selected rows only compare VGA-byte widths.
      if(spans.back().left > spans.back().right) return;
      if(spans.back().row & 1) --index;
      if(index < 0 || spans[static_cast<unsigned int>(index)].left > spans[static_cast<unsigned int>(index)].right) return;
    }
    auto const valid{[&] {
      if(index < 0) return false;
      auto const &span{spans[static_cast<unsigned int>(index)]};
      return span.left / 4 <= span.right / 4;
    }};
    auto const width{[&] { auto const &span{spans[static_cast<unsigned int>(index)]};return span.right / 4 - span.left / 4; }};
    auto const advance{[&] {
      // Skipped physical rows test pixel width; selected rows test byte width.
      for(int row{1}; row <= stride; ++row) {
        if(--index < 0) return;
        auto const &span{spans[static_cast<unsigned int>(index)]};
        if(row < stride && span.left > span.right) { index = -1;return; }
      }
    }};
    while(valid()) {
      if(width() == 0) {
        if(mode == 3) { offset = !offset;advance();continue; }
        advance();
        while(valid() && width() == 0) { offset = !offset;advance(); }
        if(!valid()) return;
      }
      auto const &span{spans[static_cast<unsigned int>(index)]};
      int const count{mode == 2 ? width() / 2 + (offset ? 0 : 1) : (width() - 1) / 4 + 1};
      int const first{span.left / 4 + (offset ? (mode == 2 ? 1 : 2) : 0)};
      for(int i{0}; i != count; ++i) {
        int const x{(first + i * (mode == 2 ? 2 : 4)) * 4 + 3};
        // Plane-three writes may cross the nominal right edge or wrap into
        // the next physical row, as in the original mode-X aperture.
        int const at{span.row * screen_width + x};
        if(at >= 0 && at < screen_width * screen_height) output.write(static_cast<unsigned int>(at),colour);
      }
      offset = !offset;advance();
    }
    return;
  }
  for(auto const &span : spans | std::views::reverse) {
    // The procedural callback tests VGA-byte width. An inverted pixel span
    // inside one byte still consumes a random mask, even though it paints none.
    if(mode == -1) {
      // The flat unrolled table skips its first negative byte-width entry.
      if((span.right | 3) + 4 < span.left) return;
    } else if(mode == 0 || mode == 1 || mode == 10 || mode == 11 || mode == 12) {
      // Dense stipple and logical painters compare byte width directly. Empty
      // pixel spans in one byte continue; an inverted byte boundary ends it.
      if((span.right | 3) < span.left) return;
    } else if(span.left > span.right && (mode != 4 || span.left / 4 > span.right / 4)) return;
    pattern_span(output,seed,span.row,span.left,span.right,colour,mode,spans.back().row,true);
  }
}

} // namespace tfx::render
