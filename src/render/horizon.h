#pragma once

#include <vector>
#include "render/framebuffer.h"
#include "render/camera.h"

namespace tfx::render {

class horizon_phase {
private:
  int16_t counter;
  uint16_t period_mode;
public:
  explicit horizon_phase(int32_t const packed = 0) : counter{math::signed_word(static_cast<uint16_t>(packed))}, period_mode{static_cast<uint16_t>(static_cast<uint32_t>(packed) >> 16)} {}
  void advance(int16_t movement);
  int32_t packed_state() const { return std::bit_cast<int32_t>((static_cast<uint32_t>(period_mode) << 16) | static_cast<uint16_t>(counter)); }
  int16_t sample() const { return counter; }
};

struct horizon_context {
  math::angle pitch{0}, roll{0};
  int16_t camera_y{0}, ground_phase{0};
  vec2<int16_t> minimum{0,0}, maximum{screen_width - 1,screen_height - 1};
  int16_t origin_y{centre_y};
  vec2<unsigned int> projection_shift{8u,8u};
};
struct horizon_surface {
  uint8_t colour;
  std::vector<vec2<int16_t>> vertices;
  bool rectangle{false};
};
// Native 0xcaf84: original projected sky/ground bands and banked edge caps.
std::vector<horizon_surface> horizon_surfaces(horizon_context const &context);

void paint_horizon(framebuffer &output, horizon_context const &context);
void paint_horizon(framebuffer &output, camera const &view, int16_t ground_phase);

} // namespace tfx::render
