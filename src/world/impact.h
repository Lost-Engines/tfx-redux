#pragma once

#include <optional>
#include "render/framebuffer.h"
#include "world/effects.h"

namespace tfx::world {

enum class impact_surface : int16_t {
  unknown = -1,
  terrain,
  structure,
  water
};
enum class crash_appearance {
  land,
  water
};
enum class player_contact_action {
  none,
  restore_destroyed,
  explode
};
struct player_contact_context {
  bool previously_destroyed;
  bool destroyed;
  bool contact_explosion;
  bool pilot_active;
};
struct player_contact_effects {
  player_contact_action action{player_contact_action::none};
  bool collision{false};
  bool release_motion_hold{false};
};

struct projectile_impact_context {
  uint8_t weapon;
  bool sample_surface;
  bool player_owned;
  std::optional<effect_kind> sampled_effect;
};
struct projectile_impact_effects {
  std::optional<effect_kind> visual{};
  bool explosion{false};
};

std::optional<effect_kind> select_impact_effect(impact_surface surface, bool cannon);
projectile_impact_effects select_projectile_impact(int16_t &appearance, projectile_impact_context const &context);

player_contact_effects resolve_player_contact(player_contact_context const &context);

impact_surface classify_impact(render::framebuffer const &frame, std::optional<vec2<int16_t>> const &point, bool water_world);
crash_appearance select_crash_appearance(int16_t collision, std::optional<int16_t> terrain_type, impact_surface sampled_surface, bool water_world);

} // namespace tfx::world
