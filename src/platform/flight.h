#pragma once

#include "actors/air.h"
#include "audio/runtime.h"
#include "cockpit/tactical_map.h"
#include "flight/cheat.h"
#include "missions/rules.h"
#include "missions/initialisation.h"
#include "configuration/settings.h"
#include "platform/window.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "simulation/timing.h"
#include "world/flak.h"
#include "world/animation.h"
#include "sensors/air.h"
#include "world/sky.h"
#include "render/horizon.h"
#include "render/precipitation.h"
#include "render/view_settings.h"
#include "configuration/graphics.h"
#include "weapons/loadout.h"

namespace tfx::platform {

struct flight_options {
  std::string mission{"miss501"};
  std::filesystem::path data_directory{};
  std::string environment{}, grid{};
  std::optional<vec2<unsigned int>> cell{};
  std::optional<std::filesystem::path> screenshot{};
  std::optional<unsigned int> joystick{};
  unsigned int scale{0}, scene_radius{6}, frame_limit{0}, flight_steps{0}, flight_frames{0};
  configuration::settings &preferences;
  world::flak_state &flak;
  world::sky_state &sky;
  world::animation_clock &scenery_clock;
  sensors::air_lock &seeker;
  vec2<int16_t> &projection_origin;
  render::horizon_phase &ground_phase;
  vec3<int32_t> &ground_reference;
  actors::air_contact &player_contact;
  int16_t &nearby_hostile_count;
  int16_t &last_released_mount;
  bool &flight_information;
  bool &cannon_navigation;
  cockpit::tactical_map_animation &map_animation;
  flight::cheat_code &cheat;
  missions::inherited_setup &mission_state;
  std::string &recording_name;
  weapons::pylon_inventory &launch_inventory;
  render::precipitation &rain;
  math::random_state &session_random;
  audio::runtime *sound{nullptr};
  bool inspect{false};
  render::view_settings video{};
  configuration::graphics_settings *graphics{nullptr}; // Shared across campaign missions.
#ifndef NDEBUG
  std::optional<std::filesystem::path> capture_recipe{};
#endif
};
enum class flight_result { finished, quit, enrol, cancelled };
// FINISH resumes the authored campaign; the settings dialog can return to enrolment.
flight_result fly_mission(resources::archive const &game, resources::executable_tables const &tables,
  flight_options const &options, missions::variables &campaign, window *shared_window = nullptr);

} // namespace tfx::platform
