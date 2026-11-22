#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "actors/air_motion.h"
#include "actors/program.h"
#include "audio/music_control.h"
#include "audio/sample_bank.h"
#include "cockpit/instrument_assets.h"
#include "flight/aerodynamic_airframes.h"
#include "flight/aerodynamic_wind.h"
#include "flight/airframes.h"
#include "flight/refuelling.h"
#include "presentation/arming_assets.h"
#include "presentation/configuration_menu.h"
#include "presentation/cursor.h"
#include "presentation/fonts.h"
#include "presentation/pilots.h"
#include "presentation/scores.h"
#include "presentation/timing.h"
#include "presentation/widget_font.h"
#include "resources/binary.h"
#include "resources/compiled_panel.h"
#include "weapons/definition.h"
#include "weapons/loadout.h"

namespace tfx::resources {

struct hud_glyph {
  unsigned int advance;
  std::array<uint8_t,6> rows;
};

class executable_tables {
private:
  int16_t static constexpr first_sight_pylon{-1};
  unsigned int static constexpr sight_pylon_count{weapons::cannon_pylon - first_sight_pylon + 1};
  std::array<std::array<vec3<int16_t>,sight_pylon_count>,3> sight_mount_data;
  std::vector<bytes> circles;
  std::unique_ptr<presentation::fonts const> font_data;
  std::unique_ptr<presentation::widget_font const> widget_font_data;
  std::unique_ptr<presentation::configuration_menu_assets const> configuration_data;
  std::unique_ptr<planner_artwork const> planner_art_data;
  std::unique_ptr<presentation::pilot_assets const> pilot_assets;
  std::unique_ptr<presentation::score_assets const> score_data;
  std::unique_ptr<presentation::arming_assets const> arming_data;
  std::unique_ptr<cockpit::instrument_assets const> instrument_data;
  std::unique_ptr<flight::refuel_assets const> refuel_data;
  std::array<uint8_t,64> character_map;
  std::array<hud_glyph,51> glyphs;
  std::array<unsigned int,256> measured_advances;

public:
  std::vector<audio::sample_definition> samples;
  audio::lapc_order_profiles music_orders;
  std::vector<weapons::definition> weapons;
  weapons::arming_tables arming;
  std::array<flight::airframe,3> airframes;
  std::array<flight::aerodynamic_airframe,3> aerodynamic_airframes;
  std::array<flight::aerodynamic_wind_parameters,flight::aerodynamic_weather_profiles> aerodynamic_wind;
  flight::aerodynamic_surface_temperatures aerodynamic_temperatures;
  std::string timer_expired_message;
  std::string fire_permission_message;
  std::string orders_violation_message;
  std::string ground_training_warning;
  std::string ground_orders_violation;
  std::string flak_warning;
  presentation::dac_palette text_palette;
  render::dac_palette fixed_flight_palette;
  uint8_t hud_green;
  std::array<presentation::pointer_style,2> pointers;
  std::vector<int16_t> ship_target_types;
  std::array<vec3<int16_t>,2> cannon_mounts;
  std::array<std::array<vec3<int16_t>,10>,3> store_mounts;
  std::array<actors::formation_offset,actors::aircraft_per_group> formations;
  std::array<uint8_t,actors::envelope_bytes> aircraft_envelopes;
  executable_tables(std::filesystem::path const &path);
  presentation::fonts const &presentation_fonts() const;
  presentation::widget_font const &interface_font() const;
  presentation::configuration_menu_assets const &configuration_widgets() const;
  planner_artwork const &planner_art() const;
  presentation::pilot_assets const &pilots() const;
  presentation::score_assets const &scores() const;
  presentation::arming_assets const &arming_art() const;
  cockpit::instrument_assets const &instruments() const;
  flight::refuel_assets const &refuel_parameters() const;
  vec3<int16_t> const &sight_mount(flight::player_aircraft aircraft, int16_t pylon) const;
  std::span<uint8_t const> circle(unsigned int radius) const;
  hud_glyph const *glyph(unsigned char character) const;
  unsigned int glyph_measure(unsigned char character) const;
};

} // namespace tfx::resources
