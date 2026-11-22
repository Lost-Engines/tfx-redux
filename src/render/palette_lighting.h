#pragma once

#include <optional>
#include "cockpit/view_control.h"
#include "render/framebuffer.h"

namespace tfx::render {

unsigned int constexpr lighting_stages{50}, altitude_palette_count{5};
enum class blackout_mode : uint8_t { none, redout, blackout };
struct palette_lighting_state {
  int16_t altitude_step{0};
  int16_t view_phase{0};
  uint8_t flash{lighting_stages};
};
struct palette_lighting_context {
  int32_t camera_y{0};
  math::angle pitch{0};
  math::angle roll{0};
  cockpit::view_mode view{cockpit::view_mode::cockpit};
  uint8_t hud_green{63};
  bool normal_changed{false};
  bool effects{true};
  blackout_mode blackout{blackout_mode::none};
  int16_t blackout_level{0};
  int16_t redout_level{0};
};
struct palette_lighting_result {
  bool changed{false};
  std::optional<unsigned int> sound{};
};
class palette_lighting {
private:
  std::array<dac_palette,altitude_palette_count> banks;
  std::array<int32_t,altitude_palette_count> altitudes;
  dac_palette fixed;
  dac_palette current;
  palette_lighting_state state;
  void blend(dac_palette const &from, dac_palette const &to, int16_t level);
  void tint(vec3<uint8_t> const &colour, int16_t level);

public:
  palette_lighting(std::array<dac_palette,altitude_palette_count> const &banks, std::array<int32_t,altitude_palette_count> const &altitudes, dac_palette const &fixed, palette_lighting_state initial = {});
  palette_lighting_state const &value() const;
  dac_palette const &colours() const;
  void flash(uint8_t level);
  palette_lighting_result advance(palette_lighting_context const &context);
};

} // namespace tfx::render
