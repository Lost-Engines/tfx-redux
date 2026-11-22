#include "colour_remap.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::render {

colour_maps::colour_maps(std::span<uint8_t const> const bytes) {
  if(!bytes.empty() && bytes.size() != colour_map_count * colour_map_size) throw std::runtime_error{"colour remap table must contain sixteen rows of 256 indices"};
  for(unsigned int row{0}; row != rows.size(); ++row) {
    for(unsigned int index{0}; index != rows[row].size(); ++index) rows[row][index] = bytes.empty() ? static_cast<uint8_t>(index) : bytes[row * colour_map_size + index];
  }
}
colour_map const &colour_maps::row(uint8_t const index) const {
  return rows.at(index);
}
uint8_t distance_colour_row(int32_t const camera_depth) {
  unsigned int constexpr depth_fraction_bits{16};
  unsigned int constexpr retail_fade_shift{9};
  return static_cast<uint8_t>(std::clamp(camera_depth >> (depth_fraction_bits + retail_fade_shift), 0, static_cast<int>(colour_map_count - 1)));
}

} // namespace tfx::render
