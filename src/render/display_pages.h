#pragma once

#include <cstdint>
#include <memory>
#include "render/framebuffer.h"

namespace tfx::render {

inline uint8_t constexpr display_page_count{3};

// The original display rotates through three retained 320x200 VGA pages.
// Pixels outside a painter's coverage survive until that page is reused.
class display_pages {
private:
  static unsigned int constexpr first_page{1};
  std::unique_ptr<std::array<framebuffer, display_page_count>> pages{std::make_unique<std::array<framebuffer, display_page_count>>()};
  unsigned int current{first_page};
  int16_t previous_offset_y{-1};

public:
  void reset();
  void begin_frame(framebuffer &target) const;
  void finish_frame(framebuffer const &target);
  // Native 74104: a changed raw Y requests all three footer copies, then the
  // shared refresh clock is decremented before overlays and instruments draw.
  bool refresh_panel(uint8_t &remaining, int16_t offset_y);
};

} // namespace tfx::render
