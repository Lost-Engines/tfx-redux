#include "impact.h"
#include <utility>
#include "weapons/definition.h"

namespace tfx::world {

std::optional<effect_kind> select_impact_effect(impact_surface const surface, bool const cannon) {
  switch(surface) {
  case impact_surface::terrain:
    return cannon ? effect_kind::cannon_hit : effect_kind::hit;
  case impact_surface::structure:
    return cannon ? effect_kind::cannon_miss : effect_kind::miss;
  case impact_surface::water:
    return cannon ? effect_kind::cannon_splash : effect_kind::splash;
  case impact_surface::unknown:
    return std::nullopt;
  }
  return std::nullopt;
}

projectile_impact_effects select_projectile_impact(int16_t &appearance, projectile_impact_context const &context) {
  // Native 0xa3423: byte 30 selects the nonsampling chaff/flare/cargo mode,
  // which inherits the preceding classifier word. Other stores replace it.
  projectile_impact_effects effects;
  if(context.sample_surface) {
    effects.visual = context.sampled_effect;
    appearance = effects.visual ? static_cast<int16_t>(std::to_underlying(*effects.visual)) : -1;
  }
  // Cannon splash is 5, so the original deliberately does not take water's
  // ordinary class-2 sound exemption for cannon effects.
  effects.explosion = appearance != -1 && appearance != std::to_underlying(effect_kind::splash) && context.player_owned && context.weapon != std::to_underlying(weapons::weapon_id::chaff) && context.weapon != std::to_underlying(weapons::weapon_id::flare);
  return effects;
}

player_contact_effects resolve_player_contact(player_contact_context const &context) {
  // Native 0x7145d: contact entry 2 runs on a temporarily restored falling aircraft.
  // No new hit restores its fatal state; another fatal hit switches to kapow.
  player_contact_effects effects;
  if(context.previously_destroyed) {
    if(!context.destroyed) {
      effects.action = player_contact_action::restore_destroyed;
    } else if(!context.contact_explosion) {
      effects.collision = true;
      if(!context.pilot_active) effects.action = player_contact_action::explode;
    }
  } else if(!context.destroyed) {
    effects.release_motion_hold = true;
  } else if(!context.contact_explosion && !context.pilot_active) {
    effects.collision = true;
    effects.action = player_contact_action::explode;
  }
  return effects;
}

impact_surface classify_impact(render::framebuffer const &frame, std::optional<vec2<int16_t>> const &point, bool const water_world) {
  // Native 0x7da20 deliberately classifies a pixel from the rendered image, rather than querying collision geometry.
  if(!point || point->x <= 0 || point->x >= render::screen_width || point->y <= 0 || point->y >= render::screen_height) return impact_surface::unknown;
  auto const pixel{frame.pixels[static_cast<unsigned int>(point->y * render::screen_width + point->x)]};
  impact_surface surface{impact_surface::unknown};
  if((pixel >= 0xb0 && pixel <= 0xdf) || (pixel >= 0x90 && pixel <= 0x9f)) surface = impact_surface::terrain;
  if(pixel >= 0x70 && pixel <= 0x7f) surface = impact_surface::water;
  if((pixel >= 0xa0 && pixel <= 0xaf) || (pixel >= 0x20 && pixel <= 0x2f) || (pixel >= 0xe0 && pixel <= 0xf0)) surface = impact_surface::structure;
  if(water_world && surface != impact_surface::unknown) surface = impact_surface::water;
  return surface;
}

crash_appearance select_crash_appearance(int16_t const collision, std::optional<int16_t> const terrain_type, impact_surface const sampled_surface, bool const water_world) {
  // Native 0x7d48a..0x7d4e9: collision 2 bypasses even the forced-water choice.
  int16_t constexpr land_collision{2};
  int16_t constexpr sea_definition_type{1};
  if(collision != land_collision && ((terrain_type && *terrain_type == sea_definition_type) || sampled_surface == impact_surface::water || water_world)) return crash_appearance::water;
  return crash_appearance::land;
}

} // namespace tfx::world
