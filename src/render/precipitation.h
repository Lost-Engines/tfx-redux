#pragma once

#include <optional>
#include "cockpit/view_control.h"
#include "render/framebuffer.h"

namespace tfx::render {

unsigned int constexpr precipitation_particles{300};
struct precipitation_gate {
  bool enabled{true};
  bool storm{false};
  int16_t layer_y{0};
  float player_y{0};
  cockpit::view_mode view{cockpit::view_mode::cockpit};
  int32_t speed{0};
};
// Native frame gate at 0x722c2. Zero is a rendered frame with no particles;
// absence means the field (including its drift) must not advance.
std::optional<int16_t> precipitation_count(precipitation_gate const &context);

struct precipitation_view {
  mat3<int32_t> const &matrix;
  vec3<int16_t> camera{0, 0, 0};
  vec2<int16_t> origin{centre_x, centre_y};
  vec2<int16_t> minimum{0, 0};
  vec2<int16_t> maximum{screen_width - 1, screen_height - 1};
};
class precipitation {
private:
  std::array<vec3<int16_t>, precipitation_particles> particles{};
  vec3<int16_t> offset;
public:
  explicit precipitation(vec3<int16_t> const &drift = {0, 0, 0});
  // Native 0x78af8 replaces the field, consuming 1,800 shared random draws.
  // It deliberately retains the three drift offsets across mission changes.
  void initialise(math::random_state &random);
  std::span<vec3<int16_t> const> field() const;
  vec3<int16_t> const &drift() const;
  // Native 0x78d8c. Wind is world X/Z, separately from physics N/E/D order.
  void paint(framebuffer &output, precipitation_view const &view, int16_t count, vec2f const &wind_xz);
};

} // namespace tfx::render
