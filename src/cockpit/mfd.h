#pragma once

#include "cockpit/control_state.h"
#include "cockpit/instrument_assets.h"
#include "flight/damage.h"
#include "render/software_renderer.h"
#include "resources/archive.h"
#include "weapons/loadout.h"

namespace tfx::cockpit {

enum class display_side : unsigned int {
  left,
  right
};
enum class display_page {
  stores,
  gauges,
  flight_data,
  fuel,
  systems,
  schematic,
  camera,
  ground_database,
  controls,
  moving_map,
  blank,
  engine_load
};
display_page outer_page(display_side side, unsigned int selection);

struct mfd_state {
  flight::player_aircraft aircraft{flight::player_aircraft::eurofighter};
  std::array<flight::system_condition, flight::aircraft_system_count> systems{};
  int32_t speed{0};
  float height{0};                                                             // Original down-positive floating world Y, before the feet conversion.
  math::angle heading{0};
  double fuel{0};
  int16_t fuel_gauge{0};                                                       // Fuel scaled to 0..9999 across the four drawn cells.
  int16_t smoothed_fuel_burn{0};
  float throttle{0};
  std::array<double, 2> engine_load{};
  weapons::armament const *inventory{nullptr};
  bool confirmed_target{false};
  control_display_state controls;
  vec2<int16_t> offset{0, 0};
};

class mfd {
private:
  instrument_assets const &assets;
  std::span<weapons::definition const> weapons;
  std::vector<resources::indexed_image> backgrounds;
  std::vector<resources::indexed_image> control_backgrounds;

public:
  mfd(instrument_assets const &assets, resources::archive const &game, std::span<weapons::definition const> weapons);
  void draw(render::software_renderer &renderer, display_side side, display_page page, mfd_state const &state, render::framebuffer *coverage = nullptr) const;
};

} // namespace tfx::cockpit
