#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include "render/software_renderer.h"
#include "render/model_queue.h"
#include "render/model_jitter.h"
#include "world/animation.h"
#include "world/assets.h"
#include "world/environment.h"
#include "world/instance.h"
#include "world/effects.h"
#include "flight/landing.h"
#include "recording/file.h"

namespace tfx::world {

struct scene_handle {
  unsigned int value;
  bool operator==(scene_handle const&) const = default;
};
enum class scene_pass { primary, instrument };
enum class submission_phase { before_sky, before_actors, after_actors };
struct scene_events {
  bool animated{false};
  bool eject_player{false};
};

class scene {
private:
  environment const &terrain;
  environment const *upper_terrain;
  assets &resources;
  std::map<unsigned int,instance> instances, upper_instances;
  struct moving_instance {
    instance state;
    vec3d position;
    vec3<math::angle> angles;
    bool visible{true};
    std::optional<unsigned int> preview_entry{};
    bool aircraft{false};
  };
  std::map<unsigned int,moving_instance> actors;
  std::vector<scene_handle> actor_order;
  unsigned int next_actor{0};
  instance &terrain_instance(unsigned int x, unsigned int z);
  std::function<void(effect_record const&)> effect_sink{};
  vec3<int32_t> const *query_reference{nullptr}; // Borrowed from the mission player.
  vec3<int16_t> reference_words(vec3d const &fallback = {0,0,0}) const;
  void emit_effect(scene_request const &request, vec3d const &origin, math::angle heading = 0);
  bool advance_instance(instance &state, vec3d const &origin, math::angle heading = 0);
  struct actor_submission {
    scene_handle handle;
    vec3d position;
    vec3<math::angle> angles;
    std::optional<vec3<int32_t>> distance_reference{};
    submission_phase phase{submission_phase::after_actors};
  };
  std::vector<actor_submission> submissions;
  struct model_submission {
    model_part part;
    vec3d position;
  };
  std::vector<model_submission> model_submissions;
  render::model_slot_metadata model_slots;
  std::vector<models::shape const*> carried_models;
  std::array<uint8_t,part_parameters> model_parameters{};
  animation_clock local_clock;
  animation_clock &clock; // Optional application-owned gate survives mission changes.
  bool animation_pending{false};
  bool render_paused{false};
  bool player_animation_pending{false}; // The player stream advances after drawing, independently of viewport consumption.
  bool instrument_animation{false};
  render::model_jitter jitter;
  struct player_rendering {
    scene_handle handle;
    vec3d position;
    vec3<math::angle> angles;
    animation_policy animation;
  };
  std::optional<player_rendering> player_model;
  std::optional<render::camera> installed_camera;
  double visual_lod_scale{1};
  std::function<render::texture_overlay(std::string_view,models::texture_binding const&)> texture_overlay;

public:
  std::set<std::string> warnings;
  unsigned int parts_drawn{0};
  unsigned int primitives_drawn{0};
  scene(environment const &terrain, assets &resources, environment const *upper = nullptr, animation_clock *retained_clock = nullptr);
  scene(scene const&) = delete;
  scene &operator=(scene const&) = delete;
  void set_texture_overlay(decltype(texture_overlay) provider) { texture_overlay=std::move(provider); }
  void set_visual_lod_scale(double scale) { visual_lod_scale = scale; }
  void set_render_paused(bool paused);
  void set_effect_sink(std::function<void(effect_record const&)> sink);
  void set_query_reference(vec3<int32_t> const *position);
  void set_carried_models(std::span<models::shape const *const> models);
  scene_handle create_actor(std::string const &definition, vec3d const &position, vec3<math::angle> const &angles, bool aircraft = false);
  // Submit these actors first; other actors retain their existing order.
  // Allocation/animation/collision identities are independent of drawing.
  void set_actor_draw_order(std::span<scene_handle const> order);
  void place_actor(scene_handle handle, vec3d const &position, vec3<math::angle> const &angles);
  void show_actor(scene_handle handle, bool visible);
  bool terrain_intact(unsigned int x, unsigned int z);
  bool terrain_destroyed(vec3<int32_t> const &position);
  instance_result collide_terrain(vec3<int32_t> const &previous, vec3<int32_t> const &current, uint16_t damage = 0, bool player = true, std::function<void(uint16_t)> response = {});
  instance_result collide_actor(scene_handle handle, vec3<int32_t> const &previous, vec3<int32_t> const &current, uint16_t damage, std::function<void(uint16_t)> response = {}, std::function<void(int)> damage_response = {}, bool player = false);
  void apply_actor_contact_damage(scene_handle handle, uint16_t damage);
  bool actor_destroyed(scene_handle handle) const;
  int16_t actor_type(scene_handle handle) const;
  int16_t terrain_type(vec3<int32_t> const &position);
  std::optional<flight::landing_port> terrain_landing_port(vec2<int16_t> const &cell);
  std::optional<flight::landing_port> actor_landing_port(scene_handle handle) const;
  vec2<int16_t> actor_target_offset(scene_handle handle) const;
  vec2<int16_t> terrain_target_offset(vec2<int32_t> const &cell);
  std::vector<vec3<int16_t>> actor_damage_centres(scene_handle handle) const;
  uint16_t actor_impact_damage(scene_handle handle) const;
  // An optional query reference is copied into the deferred submission.
  void queue_actor(scene_handle handle, vec3d const &position, vec3<math::angle> const &angles, vec3<int32_t> const *distance_reference = nullptr, submission_phase phase = submission_phase::after_actors);
  void queue_model(std::string const &name, vec3d const &position);
  instance_result actor_event(scene_handle handle, unsigned int entry);
  void prepare_terrain_cell(unsigned int x, unsigned int z);
  instance_result terrain_event(unsigned int x, unsigned int z, unsigned int entry);
  scene_events advance(timer_ticks elapsed);
  bool animate_player_model(scene_handle handle);
  void set_player_rendering(scene_handle handle, vec3d const &position, vec3<math::angle> const &angles, animation_policy policy);
  std::optional<render::camera> const &last_camera() const;
  scene_events draw(render::software_renderer &renderer, render::camera const &view, unsigned int radius, std::function<void()> const &final_submissions = {}, std::optional<int32_t> camera_anchor_y = {}, int16_t shadow_ground_height = 0, scene_pass pass = scene_pass::primary, recording::scene_frame *capture = nullptr, recording::scene_frame const *playback = nullptr);
  void replay(render::software_renderer &renderer, render::camera const &view, recording::scene_frame const &source);
};

} // namespace tfx::world
