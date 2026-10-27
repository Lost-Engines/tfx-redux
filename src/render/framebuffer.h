#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <vector>
#include "vectorstorm/vector/vector4.h"
#include "math/native.h"

namespace tfx::cockpit { class vector_hud; }

namespace tfx::render {

int constexpr screen_width{320}, screen_height{200};
int constexpr display_width{screen_width}, display_height{screen_height};
int constexpr centre_x{screen_width / 2}, centre_y{screen_height / 2};
double constexpr focal_length{128};
using palette = std::array<vec3<uint8_t>, 256>;
using dac_palette = std::array<uint8_t,256 * 3>;
using rgba_pixel = vec4<uint8_t>;
static_assert(sizeof(rgba_pixel) == 4);

// Contiguous palette indices with the same small fill/range interface as the
// original array. Dimensions belong to each target, never to VGA resource data.
class pixel_buffer : public std::vector<uint8_t> {
public:
  explicit pixel_buffer(size_t count = screen_width * screen_height) : std::vector<uint8_t>(count) {}
  void fill(uint8_t colour) { std::ranges::fill(*this,colour); }
};

struct framebuffer {
  vec2<int> size{screen_width,screen_height};
  pixel_buffer pixels{static_cast<size_t>(size.x) * static_cast<size_t>(size.y)};
  std::vector<uint8_t> coverage{};
  std::vector<uint8_t> hud_coverage{}; // Optional antialiased additive HUD light.
  cockpit::vector_hud *hud_capture{nullptr}; // Scoped, non-owning drawing sink.
  static size_t pixel_count(vec2<int> const &dimensions);
  void resize(vec2<int> const &dimensions);
  void track_writes();
  void write(size_t offset, uint8_t colour) {
    pixels[offset] = colour;
    if(!coverage.empty()) coverage[offset] = 255;
  }
  std::vector<rgba_pixel> colourise(palette const &colours) const;
  void screenshot(std::filesystem::path const &path, palette const &colours) const;
};

palette decode_palette(std::span<uint8_t const> source);
palette expand_dac_palette(dac_palette const &source);

} // namespace tfx::render
