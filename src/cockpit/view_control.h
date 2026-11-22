#pragma once

#include <optional>
#include "math/attitude.h"

namespace tfx::cockpit {

// Values are the original viewport-table indices, including intermediate views.
enum class view_mode : int16_t {
  cockpit,
  rear_basis,
  unused_2,
  full_forward,
  chase,
  unused_5,
  missile,
  missile_infrared,
  leave_cockpit,
  virtual_cockpit,
  target_tracking,
  tactical,
  look_left,
  look_right,
  enter_cockpit,
  leave_panel,
  approach_cockpit,
  depart_cockpit,
  centre_head,
  designator,
  fly_past,
  ejection,
  turning_recovery,
  look_far_right,
  look_far_left,
  impact_orbit,
  rear,
  unbanked_chase,
  target,
};
unsigned int constexpr view_queue_capacity{10};

enum class view_key {
  front,
  left,
  right,
  rear,
  chase,
  fly_past,
  tactical,
  missile,
  target,
  virtual_cockpit
};
struct view_key_events {
  bool fly_past_sound{false};
};
struct view_key_context {
  bool inhibited{false};
  bool crashed{false};
  bool ejected{false};
  bool landing{false};
  bool shift{false};
  bool low_memory{false};
  int32_t speed{0};
  vec3<int32_t> position{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
  vec2<int32_t> height_bounds{-8192, -32};
  bool missile_available{false};
  bool target_available{false};
  bool target_is_ground{false};
  bool tracking_available{false};
};
struct view_control_state {
  view_mode current{view_mode::cockpit};
  view_mode previous{view_mode::cockpit};
  std::array<view_mode, view_queue_capacity> queue{};
  unsigned int written{0};
  unsigned int read{0};
  vec2<int16_t> offset{0, 28};
  vec2<int16_t> pending_offset{-1, -1};
  vec2<int16_t> saved_offset{0, -1};
  int16_t distance{0};
  int16_t pending_distance{0};
  int16_t target_distance{0};
  int16_t recenter{0};
  int16_t departure_delay{-1};
  int16_t tracking_roll{0};
  int16_t tracking_complete{0};
  math::attitude orbit{};
  math::attitude missile_orbit{};
  vec3<int32_t> fixed_position{0, 0, 0};
  uint8_t redraw{0};
  bool ready{false};
};

class view_control {
  friend class view_camera;
private:
  view_control_state state;
  void centre(view_key_context const &context);
  void save_offset();

public:
  explicit view_control(view_control_state initial = {});
  view_control_state const &value() const;
  void offset(vec2<int16_t> const &position);
  void redraw(uint8_t remaining);
  void clear_queue();
  void fixed_position(vec3<int32_t> const &position);
  void enqueue(view_mode view, int32_t speed);
  void start_impact_orbit(int32_t speed);
  void ready();
  void consume();
  view_key_events press(view_key key, view_key_context const &context);
};

} // namespace tfx::cockpit
