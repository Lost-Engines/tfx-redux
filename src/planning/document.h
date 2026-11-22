#pragma once

#include "math/native.h"
#include "resources/binary.h"

namespace tfx::planning {

unsigned int constexpr maximum_routes{100}, route_name_bytes{41};
struct mission_parameters {
  int16_t aircraft_index{-1};
  uint16_t departure_cell{0xffff};
  uint16_t primary_target_cell{0xffff};
  uint16_t additional_target_count{0};
  std::array<uint16_t, 7> additional_targets{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
  int16_t weather_index{-1};
  // Retained authoring fields whose consuming code has not yet been established.
  std::array<uint16_t, 4> retained{0xffff, 0xffff, 0xffff, 0xffff};
  explicit mission_parameters(std::span<int32_t const> fields);
  std::array<uint16_t, 16> words() const;
};
struct waypoint {
  int16_t ordinal;
  vec2<int16_t> cell;
  std::vector<std::string> commands;
};
struct route {
  std::string name;
  std::vector<waypoint> points;
};
class document {
public:
  uint16_t format;
  mission_parameters parameters;
  // Native metadata order: planner's three alternative map images, world and palette.
  std::array<std::string, 3> map_images;
  std::string world;
  std::string palette;
  std::vector<route> routes;
  std::string notes;
  explicit document(std::string_view source);
};

} // namespace tfx::planning
