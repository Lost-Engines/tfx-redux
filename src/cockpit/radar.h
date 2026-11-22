#pragma once

#include "cockpit/instrument_assets.h"
#include "flight/aircraft.h"
#include "flight/damage.h"
#include "render/software_renderer.h"
#include "resources/archive.h"
#include "weapons/definition.h"

namespace tfx::cockpit {

enum class radar_mode {
  horizontal,
  beyond_visual_range
};
struct radar_aircraft {
  vec3<int32_t> position_q8;
  bool selected{false};
};
struct radar_surface {
  vec3<int32_t> position_q8;
  int16_t type{0};
};
struct radar_projectile {
  vec3<int32_t> position_q8;
  weapons::weapon_id weapon{weapons::weapon_id::cannon};
  bool player_owned{false};
};
struct radar_state {
  vec3<int32_t> player{0, 0, 0};
  math::angle heading{0};
  flight::player_aircraft aircraft{flight::player_aircraft::eurofighter};
  flight::system_condition condition{flight::system_condition::normal};
  bool stealth{false};
  bool guns{true};
  weapons::weapon_role selected_role{weapons::weapon_role::air};
  std::span<radar_aircraft const> enemy_aircraft;
  std::span<radar_aircraft const> allied_aircraft;
  std::span<radar_surface const> enemy_surface;
  std::span<radar_surface const> allied_surface;
  std::span<radar_projectile const> projectiles;
  vec2<int16_t> offset{0, 0};
};

class radar {
private:
  instrument_assets const &assets;
  std::vector<resources::indexed_image> backgrounds;
  bool pulse{false};

public:
  radar(instrument_assets const &assets, resources::archive const &game);
  bool phase() const;
  void draw(render::software_renderer &renderer, radar_mode mode, unsigned int range, radar_state const &state, math::random_state &random, render::framebuffer *coverage = nullptr);
};

} // namespace tfx::cockpit
