#include "media.h"
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace tfx::resources {
namespace {

unsigned int constexpr palette_offset{4}, palette_bytes{256 * 3}, pixel_offset{palette_offset + palette_bytes};
unsigned int constexpr plane_count{4}, screen_pixels{render::screen_width * render::screen_height}, plane_pixels{screen_pixels / plane_count};

} // anonymous namespace

monochrome_image decode_monochrome(std::span<uint8_t const> const source) {
  binary_view const view{source};
  vec2<unsigned int> const size{view.byte(0), view.byte(1)};
  if(!size.x || !size.y) throw std::runtime_error{"empty native monochrome bitmap"};
  unsigned int const count{((size.x + 7) / 8) * size.y};
  auto const rows{view.slice(2, count)};
  if(source.size() != count + 2) throw std::runtime_error{"unused monochrome bitmap bytes"};
  return {
    .size{vec2<unsigned int>{size}},
    .rows{rows.begin(), rows.end()},
  };
}

screen_image decode_screen(std::span<uint8_t const> const source) {
  binary_view const view{source};
  if(source.size() != pixel_offset + screen_pixels || std::string_view{reinterpret_cast<char const *>(source.data()), 4} != "war ") throw std::runtime_error{"invalid native screen image"};
  screen_image result{
    .image{},
    .colours{render::decode_palette(view.slice(palette_offset, palette_bytes))},
  };
  std::ranges::copy(view.slice(pixel_offset, screen_pixels), result.image.pixels.begin());
  return result;
}

bytes deplane(std::span<uint8_t const> const source, vec2<unsigned int> const &size) {
  if(!size.x || !size.y || size.x % plane_count || static_cast<uint64_t>(size.x) * size.y != source.size()) throw std::runtime_error{"invalid four-plane image dimensions"};
  bytes result(source.size());
  unsigned int const plane_size{static_cast<unsigned int>(source.size() / plane_count)};
  for(unsigned int plane{0}; plane != plane_count; ++plane) {
    for(unsigned int pixel{0}; pixel != plane_size; ++pixel) result[pixel * plane_count + plane] = source[plane * plane_size + pixel];
  }
  return result;
}

std::vector<indexed_image> decode_sprites(std::span<uint8_t const> const source) {
  binary_view const view{source};
  unsigned int const count{view.byte(0)};
  unsigned int constexpr maximum_sprites{160};
  if(!count || count > maximum_sprites) throw std::runtime_error{"invalid sprite count"};
  unsigned int cursor{1 + count * 2};
  std::vector<indexed_image> result;
  for(unsigned int index{0}; index != count; ++index) {
    if(view.le16(1 + index * 2) != cursor) throw std::runtime_error{"sprite offsets do not cover their resource"};
    vec2<unsigned int> const size{view.byte(cursor) * 2u, view.byte(cursor + 1)};
    cursor += 2;
    unsigned int const pixels{size.x * size.y};
    result.push_back({
      .size{vec2<unsigned int>{size}},
      .pixels{deplane(view.slice(cursor, pixels), size)},
    });
    cursor += pixels;
  }
  if(cursor != source.size()) throw std::runtime_error{"unused sprite bytes"};
  return result;
}

bytes decode_animation_block(std::span<uint8_t const> const source) {
  // Native 0xde8cc: sixteen MSB-first flag bits share a stream with literal/back-reference bytes.
  binary_view const view{source};
  unsigned int cursor{0};
  unsigned int remaining{0};
  uint16_t buffer{0};
  bytes output;
  unsigned int constexpr maximum_block_bytes{262144};
  auto const byte{[&]{return view.byte(cursor++);}};
  auto const bit{[&]{
    if(!remaining) {
      buffer = view.le16(cursor);
      cursor += 2;
      remaining = 16;
    }
    unsigned int const value{static_cast<unsigned int>(buffer) >> 15};
    buffer = static_cast<uint16_t>(buffer << 1);
    --remaining;
    return value;
  }};
  auto const append{[&](uint8_t const value){
    if(output.size() == maximum_block_bytes) throw std::runtime_error{"animation block exceeds its decode limit"};
    output.push_back(value);
  }};
  for(;;) {
    if(!bit()) {
      append(byte());
      continue;
    }
    unsigned int count{2};
    std::optional<unsigned int> high;
    if(bit()) {
      if(!bit()) {
        high = 0;
      } else {
        count = 3;
        if(bit()) {
          auto const value{byte()};
          if(!value) {
            if(bit()) continue;
            if(cursor != source.size()) throw std::runtime_error{"unused compressed animation bytes"};
            return output;
          }
          count = value + 8;
        }
      }
    } else {
      count = 4 + bit();
      if(bit()) {
        count = (count - 1) * 2 + bit();
        if(count == 9) {
          unsigned int value{0};
          for(unsigned int i{0}; i != 4; ++i) value = (value << 1) | bit();
          for(unsigned int i{0}; i != (3 + value) * 4; ++i) append(byte());
          continue;
        }
      }
    }
    if(!high) {
      high = 0;
      if(bit()) {
        *high = bit();
        if(bit()) {
          *high = (*high << 1) | bit();
          *high |= 4;
          if(!bit()) *high = (*high << 1) | bit();
        } else if(!*high) *high = 2 | bit();
      }
    }
    unsigned int const distance{(*high << 8) + byte() + 1};
    if(distance > output.size()) throw std::runtime_error{"animation back-reference precedes its output"};
    for(unsigned int i{0}; i != count; ++i) append(output[output.size() - distance]);
  }
}

animation::animation(std::span<uint8_t const> const input) : source{input.begin(), input.end()} {
  binary_view const view{source};
  if(view.le16(0) != 256 || view.le16(pixel_offset) > 1) throw std::runtime_error{"invalid native animation header"};
  colours = render::decode_palette(view.slice(palette_offset, palette_bytes));
  game_frames = view.le16(2);
  initial_full = view.le16(pixel_offset) != 0;
  unsigned int constexpr maximum_frames{2000};
  if(!game_frames || game_frames > maximum_frames) throw std::runtime_error{"invalid animation frame count"};
  unsigned int at{pixel_offset + 2};
  while(at < source.size()) {
    unsigned int const size{view.le32(at)};
    at += 4;
    view.slice(at, size);
    if(!size) throw std::runtime_error{"empty animation block"};
    blocks.push_back({
      .offset{at},
      .size{size},
    });
    at += size;
  }
  if(blocks.size() < game_frames || blocks.size() > game_frames + 2) throw std::runtime_error{"animation frame table does not match its declared count"};
}

unsigned int animation::block_count() const {
  return static_cast<unsigned int>(blocks.size());
}
unsigned int animation::position() const {
  return cursor;
}

void animation::reset(render::framebuffer const *const background) {
  cursor = 0;
  if(!background) {
    planes.fill(0);
    coverage.fill(0);
    return;
  }
  for(unsigned int plane{0}; plane != plane_count; ++plane) {
    for(unsigned int pixel{0}; pixel != plane_pixels; ++pixel) planes[plane * plane_pixels + pixel] = background->pixels[pixel * plane_count + plane];
  }
  coverage.fill(255);
}

bool animation::advance() {
  if(cursor == blocks.size()) return false;
  auto const block{blocks[cursor]};
  auto const decoded{decode_animation_block(binary_view{source}.slice(block.offset, block.size))};
  if(!cursor && initial_full) {
    if(decoded.size() != screen_pixels) throw std::runtime_error{"full animation frame does not cover the screen"};
    std::ranges::copy(decoded, planes.begin());
    coverage.fill(255);
  } else {
    binary_view const view{decoded};
    unsigned int at{0};
    for(unsigned int plane{0}; plane != plane_count; ++plane) {
      unsigned int const count{view.le16(at)};
      at += 2;
      unsigned int destination{plane * plane_pixels};
      for(unsigned int run{0}; run != count; ++run) {
        destination += view.le16(at);
        unsigned int const length{view.le16(at + 2) * 2u};
        at += 4;
        if(destination > (plane + 1) * plane_pixels || length > (plane + 1) * plane_pixels - destination) throw std::runtime_error{"animation delta leaves its VGA plane"};
        std::ranges::copy(view.slice(at, length), planes.begin() + destination);
        std::fill_n(coverage.begin() + destination, length, 255);
        at += length;
        destination += length;
      }
    }
    if(at != decoded.size()) throw std::runtime_error{"unused animation delta bytes"};
  }
  ++cursor;
  return true;
}

void animation::repeat() {
  if(blocks.size() < 2) throw std::runtime_error{"animation has no repeat frame"};
  cursor = 1;                                                                  // Native repeats frame 2 onwards over the current planes, retaining the previous traversal.
}

render::framebuffer animation::image() const {
  render::framebuffer result;
  auto const pixels{deplane(planes, {render::screen_width, render::screen_height})};
  std::ranges::copy(pixels, result.pixels.begin());
  return result;
}

render::framebuffer animation::mask() const {
  render::framebuffer result;
  auto const pixels{deplane(coverage, {render::screen_width, render::screen_height})};
  std::ranges::copy(pixels, result.pixels.begin());
  return result;
}

} // namespace tfx::resources
