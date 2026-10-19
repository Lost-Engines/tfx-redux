#pragma once

#include "presentation/arming_controller.h"
#include "render/software_renderer.h"
#include "world/assets.h"
#include "world/instance.h"

namespace tfx::presentation {

class arming_preview {
private:
  world::assets resources;
  world::instance aircraft;
  world::instance miniature;
  arming_assets const &assets;
  std::span<weapons::definition const> definitions;
  math::angle thumbnail_rotation{0};
  std::array<math::angle,weapons::weapon_count> weapon_rotations{};
  void shape(render::software_renderer &renderer, models::shape const &program, render::camera const &view,
    vec3<int32_t> const &origin, vec3<math::angle> const &angles, std::span<uint8_t const> parameters,
    std::span<models::shape const *const> attachments = {});
  void object(render::software_renderer &renderer, world::instance &actor, render::camera const &view,
    vec3d const &position, vec3<math::angle> const &angles, std::span<models::shape const *const> attachments = {});

public:
  arming_preview(resources::archive const &game, resources::executable_tables const &tables, flight::player_aircraft aircraft);
  arming_preview(arming_preview const&) = delete;
  arming_preview &operator=(arming_preview const&) = delete;
  static vec3<int16_t> pointer_point(vec2<int16_t> const &position);
  std::optional<unsigned int> pick(vec3<int16_t> const &previous, vec3<int16_t> const &current);
  static vec3<int16_t> cursor_point(vec2<int16_t> const &position, arming_state const &state);
  std::optional<unsigned int> pick_cursor(vec2<int16_t> const &position, arming_state const &state);
  void start();
  void advance();
  uint8_t bay_position() const;
  void draw_models(render::software_renderer &renderer, arming_state const &state, weapons::armament const &inventory, std::span<bool const> previews);
  void draw_cursor(render::software_renderer &renderer, arming_state const &state, vec2<int16_t> const &pointer);
};

} // namespace tfx::presentation
