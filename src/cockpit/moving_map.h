#pragma once

#include <optional>
#include "render/software_renderer.h"
#include "resources/archive.h"
#include "simulation/timing.h"

namespace tfx::cockpit {

struct map_contacts {
  // Actor positions retain the native Q8 representation; map targets use integer world units.
  std::span<vec3<int32_t> const> enemy_aircraft;
  std::span<vec3<int32_t> const> allied_aircraft;
  std::span<vec3<int32_t> const> enemy_surface;
  std::span<vec3<int32_t> const> allied_surface;
};
struct map_state {
  vec3<int32_t> player{0, 0, 0};
  math::angle heading{0};
  simulation::timer_ticks elapsed{0};
  bool stealth{false};
  map_contacts contacts{};
  std::span<vec3<int32_t> const> targets;
  std::optional<vec2<int16_t>> landing_cell;
  vec2<int16_t> offset{0, 0};
};
struct map_animation {
  uint32_t ticks{0};
  uint8_t target_colour{11};
  uint8_t landing_colour{6};
};

class moving_map {
private:
  resources::indexed_image schematic;
  map_animation animation;

public:
  moving_map(resources::archive const &game, std::string const &name);
  map_animation const &state() const;
  void draw(render::software_renderer &renderer, map_state const &state, vec2<int> const &minimum, vec2<int> const &maximum, render::framebuffer *coverage = nullptr);
};

} // namespace tfx::cockpit
