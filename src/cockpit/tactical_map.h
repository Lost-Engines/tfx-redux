#pragma once

#include <optional>
#include <span>
#include <string>
#include "cockpit/target_display.h"
#include "render/software_renderer.h"
#include "resources/compiled_overlay.h"
#include "simulation/timing.h"

namespace tfx::cockpit {

struct tactical_map_aircraft {
  air_target_reading reading;
  bool selected{false};
};
struct tactical_map_snapshot {
  vec3<int32_t> player{0, 0, 0};
  math::angle heading{0};
  std::span<vec2<uint8_t> const> route{};
  std::span<vec3<int32_t> const> targets{};
  std::optional<vec2<int16_t>> landing_cell{};
  std::span<tactical_map_aircraft const> hostile_aircraft{};
  std::span<tactical_map_aircraft const> allied_aircraft{};
  std::span<vec3<int32_t> const> hostile_surface{};
  std::span<vec3<int32_t> const> allied_surface{};
};
struct tactical_map_animation {
  uint32_t ticks{0};
  uint8_t target_colour{11};
};

// The original M screen is a paused snapshot. Its borrowed contact/route spans
// remain valid until it closes; only target/runway markers change while open.
class tactical_map {
private:
  instrument_assets const &assets;
  render::software_renderer renderer;
  resources::indexed_image schematic;
  resources::compiled_overlay overlay;
  tactical_map_animation &animation;
  tactical_map_snapshot snapshot;
  auto point(vec3<int32_t> const &position, uint8_t colour)->void;
  auto target_points()->void;
  auto landing_point(uint8_t colour)->void;
  auto contact_card(tactical_map_aircraft const &aircraft, unsigned int index)->void;

public:
  tactical_map(resources::archive const &game, resources::executable_tables const &tables,
    std::string const &schematic_name, tactical_map_animation &animation);
  auto open(tactical_map_snapshot const &state)->void;
  auto advance(simulation::timer_ticks elapsed)->void;
  auto image() const->render::framebuffer const &;
};

} // namespace tfx::cockpit
