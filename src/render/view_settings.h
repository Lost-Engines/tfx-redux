#pragma once

#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#include "render/camera.h"

namespace tfx::render {

struct view_settings {
  vec2<int> resolution{1280,800};
  bool automatic_resolution{true}; // Windows follow framebuffer pixels unless --resolution was supplied.
  double field_of_view{64.01076641616699}; // 2 atan(160 / 256), original horizontal FOV.
  unsigned int draw_distance{16}; // Terrain cells; zero draws the complete map.
  double lod_scale{.1}; // Zero holds the most detailed visual representation.
};

struct interface_layout {
  double scale;
  vec2d offset;
  explicit interface_layout(vec2<int> const &size) :
    scale{std::min(size.x / static_cast<double>(screen_width),size.y / static_cast<double>(screen_height))},
    offset{(size.x - screen_width * scale) / 2,(size.y - screen_height * scale) / 2} {}
  vec2d project(vec2d const &point) const { return offset + point * scale; }
};

inline camera scene_camera(camera const &source, view_settings const &settings) {
  auto result{source};
  interface_layout const layout{settings.resolution};
  result.projection_origin = layout.project(source.projection_origin);
  result.focal_distance = settings.resolution.x / (2 * std::tan(settings.field_of_view * std::numbers::pi / 360));
  if(source.opening == aperture::missile_nose) result.focal_distance /= 2;
  result.projection = projection_mode::continuous;
  result.viewport_minimum = {0,0};
  result.viewport_maximum = {static_cast<int16_t>(settings.resolution.x - 1),static_cast<int16_t>(std::clamp(layout.project(vec2d{source.viewport_maximum}).y,0.0,static_cast<double>(settings.resolution.y - 1)))};
  result.far_distance = settings.draw_distance ? settings.draw_distance * 2048.0 : std::numeric_limits<double>::infinity();
  return result;
}

struct scene_region {
  vec2<int> position;
  vec2<int> size;
  camera view;
};

// Embedded cameras retain their authored lens. Only their raster resolution
// and visibility settings change, independently of the main camera's FOV.
inline std::optional<scene_region> instrument_camera(camera const &source, view_settings const &settings, vec2<int> const &minimum, vec2<int> const &maximum) {
  interface_layout const layout{settings.resolution};
  auto const low{layout.project(vec2d{minimum})};
  auto const high{layout.project(vec2d{maximum} + vec2d{1,1})};
  vec2<int> const begin{std::clamp(static_cast<int>(std::ceil(low.x)),0,settings.resolution.x),std::clamp(static_cast<int>(std::ceil(low.y)),0,settings.resolution.y)};
  vec2<int> const end{std::clamp(static_cast<int>(std::ceil(high.x)),0,settings.resolution.x),std::clamp(static_cast<int>(std::ceil(high.y)),0,settings.resolution.y)};
  if(end.x <= begin.x || end.y <= begin.y) return {};
  auto view{source};
  view.projection = projection_mode::continuous;
  view.projection_origin = layout.project(source.projection_origin) - vec2d{begin};
  view.focal_distance = source.focal_distance * layout.scale;
  view.viewport_minimum = {0,0};view.viewport_maximum = vec2<int16_t>{end - begin - vec2<int>{1,1}};
  view.far_distance = settings.draw_distance ? settings.draw_distance * 2048.0 : std::numeric_limits<double>::infinity();
  return scene_region{
    .position{vec2<int>{begin}},
    .size{end - begin},
    .view{std::move(view)},
  };
}

inline std::vector<int> interface_columns(interface_layout const &layout, int const width) {
  std::vector<int> columns(static_cast<size_t>(width));
  for(int x{0}; x != width; ++x) columns[static_cast<size_t>(x)] = static_cast<int>(std::floor((x - layout.offset.x) / layout.scale));
  return columns;
}

inline framebuffer scale_image(framebuffer const &source, vec2<int> const &size) {
  framebuffer result;result.resize(size);
  interface_layout const layout{size};
  auto const columns{interface_columns(layout,size.x)};
  for(int y{0}; y != size.y; ++y) {
    int const row{static_cast<int>(std::floor((y - layout.offset.y) / layout.scale))};
    if(row < 0 || row >= screen_height) continue;
    for(int x{0}; x != size.x; ++x) {
      int const column{columns[static_cast<size_t>(x)]};
      if(column >= 0 && column < screen_width) result.pixels[static_cast<size_t>(y * size.x + x)] = source.pixels[static_cast<size_t>(row * screen_width + column)];
    }
  }
  return result;
}

inline void composite_interface(framebuffer &target, framebuffer const &source) {
  if(source.coverage.empty()) return;
  interface_layout const layout{target.size};
  auto const columns{interface_columns(layout,target.size.x)};
  for(int y{0}; y != target.size.y; ++y) {
    int const row{static_cast<int>(std::floor((y - layout.offset.y) / layout.scale))};
    if(row < 0 || row >= screen_height) continue;
    for(int x{0}; x != target.size.x; ++x) {
      int const column{columns[static_cast<size_t>(x)]};
      if(column < 0 || column >= screen_width) continue;
      auto const at{static_cast<size_t>(row * screen_width + column)};
      if(source.coverage[at]) target.pixels[static_cast<size_t>(y * target.size.x + x)] = source.pixels[at];
    }
  }
}

} // namespace tfx::render
