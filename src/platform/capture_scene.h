#pragma once
#ifndef NDEBUG
#include <filesystem>
#include <optional>
#include <vector>
#include "cockpit/view_control.h"
#include "missions/initialisation.h"
#include "render/camera.h"

namespace tfx::platform {
// Local, deterministic photographic staging. Never part of release gameplay.
struct capture_actor {
  std::string definition;
  vec3d position;
  vec3<math::angle> angles;
  unsigned int entry{0};
};
enum class capture_event_target {
  player,
  actor,
  terrain
};
struct capture_event {
  unsigned int frame;
  unsigned int entry;
  capture_event_target target;
  unsigned int actor{0};
  unsigned int x{0};
  unsigned int z{0};
};
struct capture_scene {
  std::optional<vec3<int32_t>> position;
  std::optional<vec3<math::angle>> angles;
  std::optional<flight::player_aircraft> aircraft;
  std::optional<float> speed;
  cockpit::view_mode view{cockpit::view_mode::virtual_cockpit};
  vec2d look{0, 0};                                                            // Degrees right/down relative to the native pilot camera.
  std::optional<render::camera> camera;
  std::string hud{"default"};
  std::optional<vec3<unsigned int>> displays;
  unsigned int warmup_frames{24};
  bool hide_player{false};
  bool mission_actors{true};
  std::vector<capture_actor> actors;
  std::vector<unsigned int> player_events;
  std::vector<capture_event> events;
  explicit capture_scene(std::filesystem::path const &path);
  void initialise(missions::setup &setup) const;
};
} // namespace tfx::platform
#endif // NDEBUG
