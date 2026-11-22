#pragma once

#include <array>
#include <span>
#include <vector>
#include "resources/compiled_panel.h"

namespace tfx::resources {

// Restricted decoding of the bitmap compiler's literal pixel/plane stream.
// Native column entry tables become data; no x86 instructions run during painting.
class compiled_overlay {
private:
  struct pixel_write {
    uint16_t address;
    uint8_t colour;
    uint8_t planes;
    unsigned int column;
  };
  struct column_entry {
    unsigned int first;
    unsigned int column;
  };
  std::vector<pixel_write> pixels;
  std::array<column_entry,159> entries{};                                      // Native pans -79..79 VGA byte columns.
  bool movable;

public:
  explicit compiled_overlay(std::span<uint8_t const> source);
  screen_overlay image(int horizontal_bytes = 0, int vertical_rows = 0) const;
};

} // namespace tfx::resources
