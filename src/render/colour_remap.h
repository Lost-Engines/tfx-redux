#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace tfx::render {

unsigned int constexpr colour_map_size{256}, colour_map_count{16};
using colour_map = std::array<uint8_t, colour_map_size>;
class colour_maps {
private:
  std::array<colour_map, colour_map_count> rows;
public:
  // Native D7954 starts with identities before loading root coltab.dat.
  explicit colour_maps(std::span<uint8_t const> bytes = {});
  colour_map const &row(uint8_t index) const;
};
// Retail TFX configures C54F6(9). The queued camera coordinate is the original
// raw matrix product; its signed high word selects a single row per assembly.
uint8_t distance_colour_row(int32_t camera_depth);

} // namespace tfx::render
