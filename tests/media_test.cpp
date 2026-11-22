#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "render/sprites.h"
#include "resources/media.h"

namespace {

void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
template<typename function> void rejects(function const &action) {
  try {
    action();
  } catch(std::runtime_error const &) {
    return;
  }
  throw std::runtime_error{"malformed media was accepted"};
}
void word(tfx::resources::bytes &bytes, unsigned int const at, uint32_t const value, unsigned int const size = 2) {
  for(unsigned int index{0}; index != size; ++index) bytes.at(at + index) = static_cast<uint8_t>(value >> (index * 8));
}
tfx::resources::bytes literal_block(std::span<uint8_t const> const input) {
  // A test-authored stream of literals followed by the decoder's explicit end marker.
  tfx::resources::bytes result;
  unsigned int remaining{0};
  unsigned int at{0};
  uint16_t bits{0};
  auto const bit{[&](bool const value){
    if(!remaining) {
      at = static_cast<unsigned int>(result.size());
      result.resize(result.size() + 2);
      remaining = 16;
      bits = 0;
    }
    bits = static_cast<uint16_t>(bits | (static_cast<unsigned int>(value) << --remaining));
    word(result, at, bits);
  }};
  for(auto const value : input) {
    bit(false);
    result.push_back(value);
  }
  for(unsigned int i{0}; i != 4; ++i) bit(true);
  result.push_back(0);
  bit(false);
  return result;
}
tfx::resources::bytes animation(bool const full, std::span<uint8_t const> const data) {
  auto const packed{literal_block(data)};
  tfx::resources::bytes result(778);
  word(result, 0, 256);
  word(result, 2, 1);
  word(result, 772, full);
  word(result, 774, static_cast<uint32_t>(packed.size()), 4);
  result.insert(result.end(), packed.begin(), packed.end());
  return result;
}

} // anonymous namespace

int main() try {
  tfx::resources::bytes pixels(64000);
  for(unsigned int i{0}; i != pixels.size(); ++i) pixels[i] = static_cast<uint8_t>(i);
  require(tfx::resources::decode_animation_block(literal_block(pixels)) == pixels, "literal bit-word refill changed bytes");
  tfx::resources::animation full{animation(true, pixels)};
  require(full.advance() && !full.advance(), "full-frame animation boundary differs");
  require(full.image().pixels[0] == 0 && full.image().pixels[1] == 128 && full.mask().pixels[63999] == 255, "full planar frame differs");
  tfx::resources::bytes delta(14);
  word(delta, 0, 1);
  word(delta, 2, 2);
  word(delta, 4, 1);
  delta[6] = 7;
  delta[7] = 8;
  tfx::resources::animation partial{animation(false, delta)};
  require(partial.advance() && partial.image().pixels[8] == 7 && partial.image().pixels[12] == 8 && partial.mask().pixels[9] == 0, "delta plane coverage differs");
  tfx::render::framebuffer background;
  background.pixels.fill(42);
  partial.reset(&background);
  partial.advance();
  require(partial.image().pixels[8] == 7 && partial.image().pixels[9] == 42 && partial.mask().pixels[9] == 255, "delta discarded the preceding scene");
  auto repeated{animation(false, delta)};
  word(repeated, 2, 2);
  auto next_delta{delta};
  word(next_delta, 2, 1);
  next_delta[6] = 9;
  next_delta[7] = 10;
  auto const packed{literal_block(next_delta)};
  auto const at{static_cast<unsigned int>(repeated.size())};
  repeated.resize(repeated.size() + 4);
  word(repeated, at, static_cast<uint32_t>(packed.size()), 4);
  repeated.insert(repeated.end(), packed.begin(), packed.end());
  tfx::resources::animation loop{repeated};
  loop.advance();
  loop.advance();
  loop.repeat();
  loop.advance();
  require(loop.position() == 2 && loop.image().pixels[12] == 8 && loop.image().pixels[4] == 9, "repeat must retain the final planes and restart at the second stored frame");
  word(delta, 2, 16000);
  rejects([&]{
    tfx::resources::animation bad{animation(false, delta)};
    bad.advance();
  });
  rejects([]{tfx::resources::decode_animation_block(tfx::resources::bytes{0, 192, 0});});
  rejects([]{tfx::resources::decode_animation_block({});});
  rejects([]{tfx::resources::decode_screen({});});
  rejects([]{tfx::resources::decode_sprites(tfx::resources::bytes{1, 3, 0, 2, 2, 0});});
  auto const bitmap{tfx::resources::decode_monochrome(tfx::resources::bytes{9, 2, 0x80, 0x80, 0x40, 0})};
  require(bitmap.size == vec2<unsigned int>{9, 2}, "monochrome dimensions lost a partial final byte");
  tfx::render::framebuffer ink;
  tfx::render::framebuffer coverage;
  ink.pixels.fill(99);
  tfx::render::draw_monochrome(ink, bitmap, {0, 0}, 0, &coverage);
  require(ink.pixels[0] == 0 && ink.pixels[8] == 0 && ink.pixels[321] == 0 && ink.pixels[1] == 99, "monochrome bit order, row stride or opaque black differs");
  require(coverage.pixels[0] == 255 && coverage.pixels[8] == 255 && coverage.pixels[321] == 255 && coverage.pixels[1] == 0, "monochrome coverage lost black ink or filled a transparent bit");
  rejects([]{tfx::resources::decode_monochrome({});});
  rejects([]{tfx::resources::decode_monochrome(tfx::resources::bytes{0, 1});});
  rejects([]{tfx::resources::decode_monochrome(tfx::resources::bytes{9, 2, 0x80, 0x80, 0x40});});
  rejects([]{tfx::resources::decode_monochrome(tfx::resources::bytes{8, 1, 0x80, 0});});
  std::cout << "Native planar images, literal refills, retained-background deltas and malformed boundaries passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
