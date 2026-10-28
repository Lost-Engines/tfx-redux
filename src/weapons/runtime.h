#pragma once

#include <array>
#include <functional>
#include <map>
#include "simulation/timing.h"
#include "weapons/projectile.h"
#include "weapons/warnings.h"

namespace tfx::weapons {

using projectile_id = uint32_t;

enum class actor_pool { player, aircraft, surface };
struct actor_identity {
  actor_pool pool;
  unsigned int index{0};
  bool operator==(actor_identity const&) const = default;
};
struct target_reference {
  vec3<int32_t> position{0,0,0};
  std::optional<actor_identity> actor{};
  std::optional<projectile_id> projectile{};
  bool untargeted{false};
  bool designated{false};
};
struct release {
  unsigned int weapon;
  vec3<int32_t> position;
  vec3<math::angle> angles;
  int32_t speed;
  actor_identity owner;
  target_reference target;
  uint8_t mount{0};
  bool jettisoned{false};
};
unsigned int constexpr projectile_capacity{98}; // The original 100-record pool reserves two list sentinels.
struct active_projectile {
  projectile_id id;
  projectile state;
  actor_identity owner;
  target_reference target;
  uint8_t mount;
  bool spent{false};
};
struct launch_mount {
  vec3<math::angle> angles;
  uint8_t selected;
};
struct runtime_callbacks {
  std::function<launch_mount()> mount_state{};
  std::function<simulation::timer_ticks()> launch_elapsed{};
  std::function<vec3<int32_t>(actor_identity)> actor_position{};
  std::function<vec3<int32_t>()> designation_position{};
  std::function<motion_context(active_projectile const&,uint16_t)> motion_inputs{};
  std::function<int32_t(active_projectile const&)> player_distance{};
  std::function<void(projectile_warning const&)> warning{};
  std::function<void(unsigned int,bool)> reset_visual{};
  std::function<void(active_projectile const&)> launched{};
  std::function<void(active_projectile const&)> target_cleared{};
  std::function<void(active_projectile const&)> trail{};
  std::function<void(active_projectile const&,int16_t&)> ground_impact{};
  std::function<void(active_projectile&,uint16_t)> contacts{};
  // Redux lets the rendering camera select trails beyond the DOS player radius.
  bool camera_controls_trails{false};
};

class runtime {
private:
  std::span<definition const> definitions;
  runtime_callbacks callbacks;
  struct pool_slot {
    active_projectile value{};
    pool_slot *previous{nullptr};
    pool_slot *next{nullptr};
  };
  std::vector<pool_slot> slots;
  std::vector<pool_slot*> free;
  pool_slot *first{nullptr};
  pool_slot *last{nullptr};
  std::map<projectile_id,active_projectile const*> active;
  void retire(pool_slot &slot);
  projectile_id next_id{1};
  vec3<int32_t> untargeted_point{0,0,0};
  void prepare_untargeted_target(projectile const &shot);
  std::optional<simulation::timer_ticks> substep{};
  warning_state warnings{};

public:
  runtime(std::span<definition const> definitions, runtime_callbacks callbacks, unsigned int capacity = projectile_capacity);
  runtime(runtime const&) = delete;
  runtime &operator=(runtime const&) = delete;
  runtime(runtime&&) = delete;
  runtime &operator=(runtime&&) = delete;
  projectile_id launch(release const &request);
  // HUD prediction uses the same constructor scratch target as live releases,
  // while leaving the live pool, projectile identities and ammunition alone.
  std::array<vec3<int32_t>,5> predict_cannon(vec3<int32_t> const &anchor_q8, vec3<math::angle> const &angles, int32_t speed, vec2<int32_t> const &height_bounds);
  vec3<int32_t> predict_bomb_impact(unsigned int weapon, vec3<int32_t> const &anchor_q8, vec3<math::angle> const &angles, int32_t speed, vec2<int32_t> const &height_bounds);
  projectile_id countermeasure(actor_identity owner, vec3<int32_t> const &position, bool radar, int16_t strength, math::random_state &random);
  std::map<projectile_id,active_projectile const*> const &projectiles() const;
  // Borrowed stable pool record: retirement leaves its address valid until this runtime is destroyed.
  active_projectile const *first_targeting_player() const;
  warning_state const &player_warnings() const;
  void advance(simulation::timer_ticks elapsed, math::random_state &random, bool tfx_effects = true);
  void finish_frame(); // Native 0xa31a0 retires spent records after their final drawing pass.
};

} // namespace tfx::weapons
