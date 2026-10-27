#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include "cockpit/auxiliary.h"
#include "cockpit/mfd.h"
#include "cockpit/moving_map.h"
#include "cockpit/radar.h"
#include "cockpit/readouts.h"
#include "cockpit/warnings.h"

namespace tfx::cockpit {

enum class centre_display : uint16_t { horizontal_radar, bvr_radar, moving_map };
struct hud_selection {
  uint16_t left{0};
  centre_display centre{centre_display::horizontal_radar};
  uint16_t right{0};
  uint16_t horizontal_range{0};
  uint16_t bvr_range{0};
  void cycle_left(bool has_ground_targets);
  void cycle_centre(flight::player_aircraft aircraft);
  void cycle_right(flight::player_aircraft aircraft, uint8_t weapon_category);
  void cycle_range();
  void normalise();
};
struct hud_contacts {
  std::span<radar_aircraft const> enemy_aircraft;
  std::span<radar_aircraft const> allied_aircraft;
  std::span<radar_surface const> enemy_surface;
  std::span<radar_surface const> allied_surface;
  std::span<radar_projectile const> projectiles;
};
struct hud_state {
  mfd_state instruments;
  vec3<int32_t> position{0,0,0};
  uint32_t ticks{0};
  uint32_t clock_offset{0};
  simulation::timer_ticks elapsed{0};
  bool ecm{false};
  bool stealth{false};
  uint8_t chaff{0};
  uint8_t flares{0};
  bool in_range{false};
  bool hook{false};
  bool fire_permission_notice{false};
  std::optional<uint8_t> tracked_weapon_category;
  bool stall_recovery{false};
  bool threat{false};
  uint8_t redraw_phase{0};
  unsigned int afterburner_pips{0};
  std::optional<uint8_t> approach_cue;
  bool approach_suppressed{false};
  bool low_speed_ground_contact{false};
  bool landed{false};
  hud_contacts contacts;
  std::span<vec3<int32_t> const> targets;
  std::optional<vec2<int16_t>> landing_cell;
  std::optional<std::string_view> radio_message{};
};
struct hud_effects {
  flight::system_condition fuel_condition;
  bool threat_sound;
};
struct hud_animation {
  warning_animation warnings;
  auxiliary_animation auxiliary;
  map_animation map;
  bool radar_pulse;
  uint8_t redraw_phase;
};

// The scene renderer supplies the FLIR/DLIR and ground-database views. Their
// cameras have a separate lifetime and must not be hidden inside a sprite painter.
using scene_display = std::function<void(display_side,display_page,mfd_state const&)>;

class hud {
private:
  instrument_assets const &assets;
  std::span<weapons::definition const> weapons;
  readouts numbers;
  mfd displays;
  moving_map map;
  radar radar_display;
  warning_lamps warnings;
  auxiliary_lamps auxiliary;
  std::array<uint8_t,2> last_outer_pages{0,0};
  uint8_t redraw_phase{0};

public:
  hud(instrument_assets const &assets, resources::archive const &game, std::span<weapons::definition const> weapons, std::string const &schematic);
  hud_animation animation() const;
  void draw_radio(render::software_renderer &renderer, std::string_view message) const;
  hud_effects draw(render::software_renderer &renderer, hud_selection &selection, hud_state const &state, math::random_state &random, render::framebuffer *coverage = nullptr, scene_display const &scene = {}, vector_hud *optical = nullptr);
};

} // namespace tfx::cockpit
