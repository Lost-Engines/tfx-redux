#pragma once

#include <cmath>
#include <numbers>
#include <limits>
#include "render/camera.h"

namespace tfx::render {

struct view_settings {
  vec2<int> resolution{1280,800};
  double field_of_view{64.01076641616699}; // 2 atan(160 / 256), original horizontal FOV.
  unsigned int draw_distance{16}; // Terrain cells; zero draws the complete map.
  double lod_scale{0}; // Zero holds the most detailed visual representation.
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

inline framebuffer scale_image(framebuffer const &source, vec2<int> const &size) {
  framebuffer result;result.resize(size);
  interface_layout const layout{size};
  for(int y{0}; y != size.y; ++y) for(int x{0}; x != size.x; ++x) {
    int const column{static_cast<int>(std::floor((x - layout.offset.x) / layout.scale))};
    int const row{static_cast<int>(std::floor((y - layout.offset.y) / layout.scale))};
    if(column >= 0 && column < screen_width && row >= 0 && row < screen_height) result.pixels[static_cast<size_t>(y * size.x + x)] = source.pixels[static_cast<size_t>(row * screen_width + column)];
  }
  return result;
}

inline void composite_interface(framebuffer &target, framebuffer const &source) {
  interface_layout const layout{target.size};
  for(int y{0}; y != target.size.y; ++y) for(int x{0}; x != target.size.x; ++x) {
    int const column{static_cast<int>(std::floor((x - layout.offset.x) / layout.scale))};
    int const row{static_cast<int>(std::floor((y - layout.offset.y) / layout.scale))};
    if(column < 0 || column >= screen_width || row < 0 || row >= screen_height) continue;
    auto const at{static_cast<size_t>(row * screen_width + column)};
    if(!source.coverage.empty() && source.coverage[at]) target.pixels[static_cast<size_t>(y * target.size.x + x)] = source.pixels[at];
  }
}

} // namespace tfx::render
