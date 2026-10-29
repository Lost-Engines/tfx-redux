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

// Invert the original pixel-to-interface mapping once per axis. Keeping the
// same floor expression preserves fractional scales and letterbox boundaries.
struct interface_raster {
  std::array<int,screen_width + 1> columns;
  std::array<int,screen_height + 1> rows;
  explicit interface_raster(vec2<int> const &size) {
    interface_layout const layout{size};
    auto const map{[&](auto &bounds, int extent, double offset) {
      int pixel{0};
      for(size_t source=0;source<bounds.size();++source) {
        while(pixel<extent && std::floor((pixel-offset)/layout.scale)<static_cast<double>(source)) ++pixel;
        bounds[source]=pixel;
      }
    }};
    map(columns,size.x,layout.offset.x);map(rows,size.y,layout.offset.y);
  }
  template<class Paint> void covered_spans(std::span<uint8_t const> coverage, Paint const &paint) const {
    for(int row=0;row<screen_height;++row) {
      if(rows[row]==rows[row+1]) continue;
      auto const *mask{coverage.data()+row*screen_width};
      for(int column=0;column<screen_width;) {
        while(column<screen_width && !mask[column]) ++column;
        int const first{column};
        while(column<screen_width && mask[column]) ++column;
        if(columns[first]<columns[column]) paint(row,first,column);
      }
    }
  }
};

inline framebuffer scale_image(framebuffer const &source, vec2<int> const &size) {
  framebuffer result;result.resize(size);
  interface_raster const mapping{size};
  for(int row=0;row<screen_height;++row) {
    int const first{mapping.rows[row]}, last{mapping.rows[row+1]};
    if(first==last) continue;
    auto *line{result.pixels.data()+first*size.x};
    for(int column=0;column<screen_width;++column)
      std::fill(line+mapping.columns[column],line+mapping.columns[column+1],source.pixels[static_cast<size_t>(row*screen_width+column)]);
    for(int y=first+1;y<last;++y) std::copy_n(line,size.x,result.pixels.data()+y*size.x);
  }
  return result;
}

inline void composite_interface(framebuffer &target, framebuffer const &source) {
  if(source.coverage.empty()) return;
  interface_raster const mapping{target.size};
  // Expand each covered source row once, then copy its runs to repeated rows.
  // Empty regions need no work proportional to the output resolution.
  std::vector<uint8_t> line(static_cast<size_t>(target.size.x));
  mapping.covered_spans(source.coverage,[&](int row,int first,int last) {
    int const left{mapping.columns[first]}, right{mapping.columns[last]};
    for(int column=first;column<last;++column)
      std::fill(line.data()+mapping.columns[column],line.data()+mapping.columns[column+1],source.pixels[static_cast<size_t>(row*screen_width+column)]);
    for(int y=mapping.rows[row];y<mapping.rows[row+1];++y) {
      auto const at{static_cast<size_t>(y*target.size.x+left)};
      std::copy_n(line.data()+left,right-left,target.pixels.data()+at);
      if(!target.hud_coverage.empty()) std::fill_n(target.hud_coverage.data()+at,right-left,uint8_t{0});
    }
  });
}

} // namespace tfx::render
