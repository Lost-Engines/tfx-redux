#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include "render/view_settings.h"

namespace tfx::configuration {

// Redux-only preferences: the original TFX.CFG remains interoperable with DOS/TFX C++.
inline constexpr char graphics_filename[]{"REDUX.CFG"};
enum class graphics_field { resolution, window_scale, field_of_view, draw_distance, lod_scale };
struct graphics_settings {
  render::view_settings video{};
  unsigned int window_scale{0};
};
// Shared by the command line, saved preferences and the runtime editor.
void set_graphics_value(graphics_settings &settings, graphics_field field, std::string_view value);
std::string graphics_value(graphics_settings const &settings, graphics_field field);
graphics_settings parse_graphics(std::string_view text);
graphics_settings load_graphics(std::filesystem::path const &directory);
void save_graphics(std::filesystem::path const &directory, graphics_settings const &settings);

} // namespace tfx::configuration
