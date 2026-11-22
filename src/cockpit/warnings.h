#pragma once

#include <optional>
#include "cockpit/instrument_assets.h"
#include "flight/damage.h"
#include "render/clipping.h"
#include "resources/archive.h"

namespace tfx::cockpit {

struct warning_state {
  std::array<flight::system_condition, flight::aircraft_system_count> systems{};
  bool autopilot{false};
  bool hook{false};
  bool fire_permission_notice{false};
  std::optional<uint8_t> tracked_weapon_category;
  bool stall_recovery{false};
  bool threat{false};
  vec2<int16_t> offset{0, 0};
};
struct warning_animation {
  uint16_t countdown{0};
  uint16_t phase{0};
  bool threat_sound_played{false};
};

class warning_lamps {
private:
  instrument_assets const &assets;
  std::array<std::vector<resources::indexed_image>, 2> banks;
  warning_animation animation;

public:
  warning_lamps(instrument_assets const &assets, resources::archive const &game);
  warning_animation const &state() const;
  bool draw(render::framebuffer &target, warning_state const &state, render::framebuffer *coverage = nullptr, render::raster_clip const &clip = {});
};

} // namespace tfx::cockpit
