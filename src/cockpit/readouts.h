#pragma once

#include "cockpit/control_state.h"
#include "cockpit/instrument_assets.h"
#include "flight/aircraft.h"
#include "render/software_renderer.h"
#include "resources/archive.h"

namespace tfx::cockpit {

unsigned int constexpr instrument_ticks_per_second{54};
enum class fuel_warning { none, low, critical };

struct readout_state {
  uint32_t elapsed_ticks{0};
  uint32_t clock_offset{0};
  double fuel{0};
  float throttle{0};
  flight::player_aircraft aircraft{flight::player_aircraft::eurofighter};
  bool ecm{false};
  bool stealth{false};
  uint8_t chaff{0};
  uint8_t flares{0};
  vec2<int16_t> offset{0,0};
};

class readouts {
private:
  instrument_assets const &assets;
  std::vector<resources::indexed_image> digits;
  std::vector<resources::indexed_image> autopilot_lamp;
  void number(render::software_renderer &renderer, vec2<int> const &position, std::string_view text, render::framebuffer *coverage) const;

public:
  readouts(instrument_assets const &assets, resources::archive const &game);
  void draw_radio(render::software_renderer &renderer, std::string_view message, bool forward, vec2<int16_t> const &offset = {0,0}, render::framebuffer *coverage = nullptr) const;
  void draw_autopilot(render::software_renderer &renderer, autopilot_display_state const &state, vec2<int16_t> const &offset = {0,0}, render::framebuffer *coverage = nullptr) const;
  fuel_warning draw(render::software_renderer &renderer, readout_state const &state, render::framebuffer *coverage = nullptr) const;
};

} // namespace tfx::cockpit
