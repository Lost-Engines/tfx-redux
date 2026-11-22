#pragma once

#include <optional>
#include "render/framebuffer.h"
#include "resources/binary.h"

namespace tfx::resources {

struct indexed_image {
  vec2<unsigned int> size;
  bytes pixels;
};
struct monochrome_image {
  vec2<unsigned int> size;
  bytes rows;
};
monochrome_image decode_monochrome(std::span<uint8_t const> source);

struct screen_image {
  render::framebuffer image;
  render::palette colours;
};
screen_image decode_screen(std::span<uint8_t const> source);
std::vector<indexed_image> decode_sprites(std::span<uint8_t const> source);
bytes deplane(std::span<uint8_t const> source, vec2<unsigned int> const &size);
bytes decode_animation_block(std::span<uint8_t const> source);

class animation {
private:
  bytes source;
  struct block {
    unsigned int offset;
    unsigned int size;
  };
  std::vector<block> blocks;
  unsigned int cursor{0};
  std::array<uint8_t, render::screen_width * render::screen_height> planes{};
  std::array<uint8_t, render::screen_width * render::screen_height> coverage{};

public:
  render::palette colours;
  unsigned int game_frames;
  bool initial_full;
  explicit animation(std::span<uint8_t const> source);
  unsigned int block_count() const;
  unsigned int position() const;
  void reset(render::framebuffer const *background = nullptr);
  bool advance();
  void repeat();
  render::framebuffer image() const;
  render::framebuffer mask() const;
};

} // namespace tfx::resources
