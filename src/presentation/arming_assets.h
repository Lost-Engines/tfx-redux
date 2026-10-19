#pragma once

#include <array>
#include <string>
#include <vector>
#include "math/native.h"
#include "resources/binary.h"
#include "flight/stores.h"

namespace tfx::presentation {

enum class arming_entry { normal, mission_incomplete, preflight, rearming };
enum class arming_action { rotate, idle, defaults, done, clear_all, clear_mount, overview, scroll_up, scroll_down, scroll_thumb, select_store };
struct arming_region {
  vec2<int16_t> minimum;
  vec2<int16_t> maximum;
  arming_action action;
  unsigned int row{0};
  bool contains(vec2<int16_t> const &point) const;
};
struct arming_assets {
  std::string backdrop;
  std::array<std::string,3> preview_names;
  std::string empty_model;
  std::array<std::string,3> aircraft_names;
  std::array<std::string,2> roles;
  std::array<std::string,14> categories;
  std::string inertia_format, drag_format, internal, maximum, mass_format, store_mass_format, store_drag_format;
  std::array<std::string,2> unavailable;
  std::string mission_incomplete;
  std::array<vec2<int16_t>,4> unavailable_box;
  std::vector<arming_region> regions;
  std::array<arming_region,6> rows;
  std::array<unsigned int,flight::store_slots> paired;
  std::array<vec2<int16_t>,flight::store_slots> slot_labels;
  std::array<vec2<int16_t>,4> scrollbar;
  explicit arming_assets(std::span<uint8_t const> executable);
};

} // namespace tfx::presentation
