#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include "math/native.h"
#include "flight/aircraft.h"
#include "resources/archive.h"

namespace tfx::world {

unsigned int constexpr map_width{200}, map_height{200};
unsigned int constexpr cell_shift{11};
int constexpr cell_units{1 << cell_shift};
uint16_t constexpr night_status{1u << 7};
uint16_t apply_detail_status(uint16_t status, uint8_t detail);

struct terrain_tile {
  math::angle rotation;
  std::vector<std::string> variants;
};
using flight::player_aircraft;

struct mission_start {
  vec3<int32_t> position;
  bool landed{false};
};
struct mission {
  std::string name;
  std::string world_name;
  std::string environment;
  std::string waypoints;
  mission_start start;
  std::optional<player_aircraft> aircraft;
  std::string script;
  mission(resources::archive const &game, std::string const &name);
  std::string grid_name() const;
};

struct lighting_configuration {
  std::string config;
  std::string palette_name;
  std::array<std::string,4> upper_palette_names;
  std::array<int32_t,5> palette_altitudes{};
  std::string infrared_palette_name;
  std::string schematic_name;
  uint16_t status{0};
  bool water_world{false};
  bool cloud_layer{false};                                                     // Native altitude-prefix flag at 0xe762.
  int16_t layer_y{0}, layer_offset_y{0};
  std::string upper_grid{};
  bool initial_cockpit_raised{false};                                          // Native environment bit 0: start with the panel fully raised.
  bool sun_suppressed{false};                                                  // Native environment bit 0x200.
  bool storm{false};                                                           // Native environment bit 0x80, separate from night status.
  lighting_configuration(resources::archive const &game, std::string const &name);
};

class environment {
private:
  std::array<uint16_t, map_width * map_height> cells;
  std::array<uint16_t, map_width * map_height> choices;
  math::random_state random;

public:
  std::string name;
  std::string lighting_name;
  lighting_configuration lighting;
  std::vector<terrain_tile> tiles;

  environment(resources::archive const &game, std::string name, std::string lighting_name = "");
  terrain_tile const &tile(unsigned int x, unsigned int z) const;
  std::string const &variant(unsigned int x, unsigned int z) const;
  unsigned int tile_id(unsigned int x, unsigned int z) const;
  math::random_state variant_random_state() const;
};

} // namespace tfx::world
