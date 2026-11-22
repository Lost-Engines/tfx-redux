#pragma once

#include <optional>
#include "cockpit/instrument_assets.h"
#include "resources/archive.h"
#include "resources/compiled_overlay.h"

namespace tfx::cockpit {

enum class panel_copy {
  preserve,
  refresh
};

class frame {
private:
  struct artwork {
    std::optional<resources::indexed_image> panel;
    std::optional<resources::compiled_overlay> overlay;
  };
  instrument_assets const &assets;
  std::array<artwork, viewport_count> views;

public:
  frame(instrument_assets const &assets, resources::archive const &game);
  vec2<int16_t> clamp(unsigned int view, vec2<int16_t> const &offset) const;
  vec2d projection_origin(unsigned int view, vec2<int16_t> const &offset) const;
  void paint(render::framebuffer &target, unsigned int view, vec2<int16_t> const &offset, render::framebuffer *coverage = nullptr, panel_copy copy = panel_copy::refresh) const;
};

enum class cockpit_motion {
  idle,
  up,
  down,
  flak_shake,
  ground_bump
};
class cockpit_position {
private:
  instrument_assets const &assets;
  vec2<int16_t> offset;
  unsigned int step{0};
  cockpit_motion motion{cockpit_motion::idle};
  vec2<int16_t> shake_origin{-1, -1};
  unsigned int shake_step{0};

public:
  cockpit_position(instrument_assets const &assets, vec2<int16_t> const &offset);
  vec2<int16_t> const &value() const;
  unsigned int step_index() const;
  cockpit_motion movement() const;
  void move_to(vec2<int16_t> const &position);
  void raise();
  void lower();
  void shake();
  void bump();
  void advance(int16_t viewport_bottom, bool forward_cockpit = true);
};

} // namespace tfx::cockpit
