#pragma once

#include <optional>
#include "cockpit/instrument_assets.h"
#include "flight/damage.h"
#include "render/clipping.h"
#include "resources/archive.h"

namespace tfx::cockpit {

struct auxiliary_state {
  std::array<flight::system_condition, 2> engines{};
  uint8_t redraw_phase{0};
  unsigned int afterburner_pips{0};
  std::optional<uint8_t> approach_cue;
  bool approach_suppressed{false};
  bool low_speed_ground_contact{false};
  bool landed{false};
  vec2<int16_t> offset{0, 0};
};
struct auxiliary_animation {
  uint8_t engine_refresh{0};
  uint8_t afterburner_refresh{3};
};

class auxiliary_lamps {
private:
  instrument_assets const &assets;
  std::array<std::vector<resources::indexed_image>, 3> banks;
  auxiliary_animation animation;

public:
  auxiliary_lamps(instrument_assets const &assets, resources::archive const &game);
  auxiliary_animation const &state() const;
  void draw(render::framebuffer &target, auxiliary_state const &state, render::framebuffer *coverage = nullptr, render::raster_clip const &clip = {});
};

} // namespace tfx::cockpit
