#include "environment.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tfx::world {
namespace {

std::string text(resources::bytes const &data) {
  return {data.begin(), data.end()};
}
std::string lower(std::string value) {
  for(auto &c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

} // anonymous namespace

mission::mission(resources::archive const &game, std::string const &source_name) : name{source_name} {
  /// Read initial scene selections only; mission expressions and completion rules remain original source text
  script = text(game.read("missions/" + name + ".mis"));
  std::istringstream source{script};
  std::string line;
  bool in_text{false};
  while(std::getline(source, line)) {
    if(auto const comment{line.find(';')}; comment != std::string::npos) line.erase(comment);
    std::istringstream fields{line};
    std::string key;
    fields >> key;
    key = lower(std::move(key));
    if(key == "finish:" || key == "end:") break;
    if(key == "end_text") {
      in_text = false;
      continue;
    }
    if(in_text) continue;
    if(key == "world") {
      fields >> world_name;
    } else if(key == "type") {
      fields >> environment;
    } else if(key == "way") {
      fields >> waypoints;
    } else if(key == "plane") {
      unsigned int value{0};
      fields >> value;
      if(value < std::to_underlying(player_aircraft::eurofighter) || value > std::to_underlying(player_aircraft::raptor)) throw std::runtime_error{"invalid player craft in " + name};
      aircraft = static_cast<player_aircraft>(value);
    } else if(key == "start") {
      std::string position;
      std::getline(fields, position);
      std::ranges::replace(position, ',', ' ');
      std::istringstream coordinates{position};
      std::string altitude;
      if(!(coordinates >> start.position.x >> altitude >> start.position.z)) throw std::runtime_error{"invalid player start in " + name};
      start.landed = altitude == "landed";
      if(!start.landed) start.position.y = std::stoi(altitude);                // numeric Y is already signed native world height, not positive altitude
    }
    std::string word;
    while(fields >> word) {
      if(lower(word) == "text") in_text = true;
    }
  }
  if(world_name.empty()) throw std::runtime_error{"mission lacks a world: " + name};
  if(environment.empty()) throw std::runtime_error{"mission lacks an environment: " + name};
}

std::string mission::grid_name() const {
  /// Original world loader terminates the DOS filename after eight characters at 2.05 file 0xb0a80
  unsigned int constexpr dos_basename_length{8};
  return lower(world_name.substr(0, dos_basename_length));
}

uint16_t apply_detail_status(uint16_t const status, uint8_t const detail) {
  uint16_t constexpr low_detail{1u << 2};
  uint16_t constexpr reduced_detail{1u << 6};
  switch(detail) {
  case 0:
    return static_cast<uint16_t>(status | reduced_detail | low_detail);
  case 1:
    return static_cast<uint16_t>((status | reduced_detail) & ~low_detail);
  case 2:
    return static_cast<uint16_t>(status & ~(low_detail | reduced_detail));
  default:
    return status;
  }
}

lighting_configuration::lighting_configuration(resources::archive const &game, std::string const &name) {
  config = text(game.read("lev/" + name + ".txt"));
  std::istringstream settings{config};
  std::string token;
  while(settings >> token) {
    token = lower(std::move(token));
    if(token == "altitude") {
      cloud_layer = true;
      int32_t height;
      int32_t offset;
      if(!(settings >> height >> offset >> upper_grid)) throw std::runtime_error{"environment lacks its altitude-layer binding: " + name};
      layer_y = math::signed_word(static_cast<uint16_t>(0u - static_cast<uint32_t>(height)));
      layer_offset_y = math::signed_word(static_cast<uint16_t>(0u - static_cast<uint32_t>(offset)));
    }
    if(token == "inferno" || token == "sea" || token == "storm" || token == "seastorm" || token == "twilight") initial_cockpit_raised = true;
    if(token == "twilight") sun_suppressed = true;
    if(token == "storm" || token == "seastorm") storm = true;
    if(token == "sea" || token == "seastorm") water_world = true;
    if(token == "night") status |= night_status;
    if(token == "night" || token == "day") {
      settings >> palette_name;
      // Native 0x747fc reads five altitude boundaries around four higher-altitude
      // palettes, followed by the infrared palette and the map sprite basename.
      for(unsigned int index{0}; index != upper_palette_names.size(); ++index) settings >> palette_altitudes[index] >> upper_palette_names[index];
      if(!(settings >> palette_altitudes.back() >> infrared_palette_name >> schematic_name)) throw std::runtime_error{"environment lacks its palette/schematic bindings: " + name};
      break;
    }
  }
  if(palette_name.empty()) throw std::runtime_error{"environment has no base day/night palette: " + name};
}

environment::environment(resources::archive const &game, std::string source_name, std::string source_lighting) :
  name{std::move(source_name)},
  lighting_name{source_lighting.empty() ? name : std::move(source_lighting)},
  lighting{game, lighting_name} {
  /// The native grid is column-major with reversed Z; retain a convenient row-major runtime lookup
  auto const prefix{"lev/" + name};
  auto const low{game.read(prefix + ".lev")};
  auto const high{game.read(prefix + ".le2")};
  if(low.size() != cells.size() || high.size() != cells.size()) throw std::runtime_error{"terrain grid extent differs from 200 by 200: " + name};
  auto const table{game.read(prefix + ".dat")};
  resources::binary_view const view{table};
  auto const count{view.le16(0)};
  size_t cursor{2};
  for(unsigned int i{0}; i != count; ++i) {
    terrain_tile tile{
      .rotation{view.le16(cursor)},
      .variants{},
    };
    auto const variants{view.le16(cursor + 2)};
    cursor += 4;
    for(unsigned int j{0}; j != variants; ++j) {
      std::string variant;
      while(view.byte(cursor)) variant += static_cast<char>(view.byte(cursor++));
      ++cursor;
      tile.variants.push_back(lower(std::move(variant)));
    }
    if(tile.variants.empty()) throw std::runtime_error{"terrain tile has no scenery variants"};
    tiles.push_back(std::move(tile));
  }
  if(cursor != table.size()) throw std::runtime_error{"terrain variant table has unexplained trailing bytes"};
  // Native 0xb0708 visits stored rows first, despite the column-major files.
  // Random draws belong to that traversal order, not the storage layout.
  for(unsigned int row{0}; row != map_height; ++row) {
    for(unsigned int x{0}; x != map_width; ++x) {
      auto const source{x * map_height + row};
      auto const target{(map_height - 1 - row) * map_width + x};
      cells[target] = static_cast<uint16_t>(low[source] | (high[source] << 8));
      if(cells[target] >= tiles.size()) throw std::runtime_error{"terrain grid references an undefined tile in " + name};
      choices[target] = static_cast<uint16_t>(random.next() % tiles[cells[target]].variants.size());
    }
  }
}

math::random_state environment::variant_random_state() const {
  return random;
}

terrain_tile const &environment::tile(unsigned int const x, unsigned int const z) const {
  return tiles.at(tile_id(x, z));
}
std::string const &environment::variant(unsigned int const x, unsigned int const z) const {
  return tile(x, z).variants.at(choices.at(z * map_width + x));
}
unsigned int environment::tile_id(unsigned int const x, unsigned int const z) const {
  if(x >= map_width || z >= map_height) throw std::out_of_range{"world cell exceeds map dimensions"};
  return cells[z * map_width + x];
}

} // namespace tfx::world
