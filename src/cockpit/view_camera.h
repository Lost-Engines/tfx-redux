#pragma once

#include "cockpit/view_control.h"
#include "render/camera.h"
#include "simulation/timing.h"

namespace tfx::cockpit {

struct missile_camera_pose {
  vec3<int32_t> position_q8{0,0,0};
  vec3<math::angle> angles{0,0,0};
};
struct view_camera_context {
  vec3<int32_t> position{0,0,0};
  vec3<int32_t> position_q8{0,0,0};
  vec3f position_f32{0,0,0};
  vec3<math::angle> angles{0,0,0};
  math::attitude basis{}; // Current renderer basis; Simple can offset it from physics.
  math::attitude const *body_basis{nullptr}; // Optional physics basis, borrowed for this update.
  simulation::timer_ticks elapsed{0};
  uint16_t pan{0};
  int16_t ground_height{0};
  bool low_memory{false};
  bool landing{false};
  uint8_t refuel_phase{0};
  int32_t speed{0};
  std::optional<vec3<int32_t>> tracking_target_q8{};
  std::optional<vec3<int32_t>> observed_target_q8{};
  std::optional<missile_camera_pose> missile{};
  std::optional<vec3<int32_t>> impact_target_q8{};
  vec2<int32_t> height_bounds{-8192,-32};
};
enum class player_model_source { fixed, integer };
struct view_pose {
  render::aperture opening{render::aperture::normal};
  std::optional<int16_t> ground_motion{};
  std::optional<vec2<math::angle>> horizon_angles{};
  int16_t horizon_camera_y{0};
  vec3<int32_t> anchor{0,0,0};
  vec3<int32_t> position_q16{0,0,0};
  vec3<math::angle> angles{0,0,0};
  mat3<int32_t> matrix{};
  std::optional<unsigned int> player_model_entry{};
  player_model_source model_source{player_model_source::fixed};
  render::camera camera(vec2d const &projection_origin) const;
};
struct view_motion_state {
  vec3<int32_t> ground_reference{0,0,0};
  vec3<int16_t> head{0,0,0};
  vec3<int32_t> offset_q16{0,0,0};
  vec3<math::angle> smoothed_angles{0,0,0};
  int16_t pitch{0};
  int16_t heading{0};
  int16_t orbit_roll{0};
  int32_t previous_floor_offset{0};
  uint16_t pan{0};
  missile_camera_pose last_missile{};
};

class view_camera {
private:
  view_motion_state state;
  view_pose pose;
  vec3d display_angles{0,0,0};
  mat3d display_basis{};
  vec3d display_offset{0,0,0};
  std::optional<vec3d> display_position;
  void pan_head(view_control &views, uint16_t pan, bool limit_pitch);
  void limit_head(view_control &views);
  void select_head_height();
  bool compose(view_control &views, view_camera_context const &context, bool chase);
  void set_angles(vec3<math::angle> const &angles);
  void player_origin(view_camera_context const &context);

public:
  explicit view_camera(view_motion_state initial = {});
  view_motion_state const &value() const;
  view_pose const &last_pose() const;
  render::camera display_camera(vec2d const &projection_origin) const;
  void initialise_ground_reference(vec3<int32_t> const &position);
  void advance_ground_reference(view_camera_context const &context, bool inhibited = false);
  bool advance(view_control &views, view_camera_context const &context);
};

} // namespace tfx::cockpit
