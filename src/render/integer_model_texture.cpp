#include "integer_model_texture.h"
#include <cmath>
#include "integer_texture_polygon.h"
#include "model_projection.h"
#include "near_clip.h"

namespace tfx::render {

bool integer_model_texture(framebuffer &output, std::span<models::model_vertex const> const vertices, camera const &view, std::span<uint8_t const> const texture, colour_map const &remap) {
  if(view.projection != projection_mode::original_model || vertices.size() < 3) return false;
  unsigned int constexpr uv_fraction_bits{7};
  std::vector<vec3<int32_t>> points;
  points.reserve(vertices.size());
  std::vector<vec2<uint16_t>> uv;
  uv.reserve(vertices.size());
  for(auto const &vertex : vertices) {
    auto const point{model_camera_point(vertex)};
    if(!point) return false;
    points.emplace_back(*point);
    uv.emplace_back(static_cast<uint16_t>(static_cast<int64_t>(std::nearbyint(vertex.uv.x * (1u << uv_fraction_bits)))), static_cast<uint16_t>(static_cast<int64_t>(std::nearbyint(vertex.uv.y * (1u << uv_fraction_bits)))));
  }
  vec2<int16_t> const origin{static_cast<int16_t>(view.projection_origin.x), static_cast<int16_t>(view.projection_origin.y)};
  std::vector<textured_vertex> projected;
  projected.reserve(vertices.size() + 2);
  for(unsigned int i{0}; i != points.size(); ++i) {
    unsigned int const next{(i + 1) % static_cast<unsigned int>(points.size())};
    auto const &a{points[i]};
    auto const &b{points[next]};
    bool const inside_a{a.z >> 16 > model_near_depth};
    bool const inside_b{b.z >> 16 > model_near_depth};
    if(inside_a) {
      auto const screen{project_model_point(a, view)};
      if(!screen) return false;
      projected.push_back({
        .position{vec2<int16_t>{*screen}},
        .uv_q7{vec2<uint16_t>{uv[i]}},
      });
    }
    if(inside_a != inside_b) {
      auto const intersection{inside_a ? intersect_textured_near_plane(b, a, uv[next], uv[i]) : intersect_textured_near_plane(a, b, uv[i], uv[next])};
      projected.push_back({
        .position{project_near_plane(intersection.position, view.opening, origin)},
        .uv_q7{vec2<uint16_t>{intersection.uv_q7}},
      });
    }
  }
  if(projected.size() >= 3) integer_texture_polygon(output, projected, texture, remap, view.viewport_minimum, view.viewport_maximum);
  return true;
}

} // namespace tfx::render
