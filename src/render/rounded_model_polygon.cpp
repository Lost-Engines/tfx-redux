#include "rounded_model_polygon.h"
#include <bit>
#include "render/integer_model_polygon.h"
#include "render/integer_polygon.h"
#include "render/model_projection.h"
#include "render/near_clip.h"
#include "render/pattern_raster.h"
#include "render/rounded_curve.h"

namespace tfx::render {
namespace {

bool rounded_facing(std::span<vec2<int16_t> const> const screen) {
  // BC660/BCA58 use dword differences except for the final Y difference,
  // which is subtracted as a word before sign extension.
  auto const &a{screen[0]};
  auto const &b{screen[1]};
  auto const &c{screen[2]};
  int32_t const y1{b.y - a.y};
  int32_t const x2{c.x - a.x};
  int32_t const x1{b.x - a.x};
  int32_t const y2{math::signed_word(static_cast<uint16_t>(c.y - a.y))};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(x1) * static_cast<uint32_t>(y2) - static_cast<uint32_t>(y1) * static_cast<uint32_t>(x2)) >= 0;
}

} // anonymous namespace

bool rounded_model_polygon(framebuffer &output, std::span<models::model_vertex const> const vertices, camera const &view, uint8_t const colour, int const mode, uint16_t &seed) {
  if(view.projection != projection_mode::original_model || (vertices.size() != 3 && vertices.size() != 4)) return false;
  if(vertices.size() == 3) {
    std::array<vec2<int16_t>, 3> screen;
    for(unsigned int i{0}; i != screen.size(); ++i) {
      auto const point{model_camera_point(vertices[i])};
      if(!point) return false;
      if(point->z >> 16 <= model_near_depth) return true;
      auto const projected{project_model_point(*point, view)};
      if(!projected) return false;
      screen[i] = *projected;
    }
    if(!rounded_facing(screen)) return true;
    auto const &a{screen[0]};
    auto const &b{screen[1]};
    auto const &c{screen[2]};
    auto const curve{rounded_arc<3>(vec2<int32_t>{a}, vec2<int32_t>{b}, vec2<int32_t>{c})};
    std::array<vec2<int16_t>, curve.size()> points;
    for(unsigned int i{0}; i != points.size(); ++i) points[i] = vec2<int16_t>{curve[i]};
    pattern_polygon(output, seed, integer_polygon_spans(points, view.viewport_minimum, view.viewport_maximum, polygon_facing::unchecked), colour, mode);
    return true;
  }
  std::array<vec3<int32_t>, 4> camera;
  for(unsigned int i{0}; i != camera.size(); ++i) {
    auto const point{model_camera_point(vertices[i])};
    if(!point) return false;
    camera[i] = *point;
    // This original consumer omits a whole rounded face when a corner is near;
    // it does not send it through the ordinary polygon near-plane clipper.
    if(camera[i].z >> 16 <= model_near_depth) return true;
  }
  auto const corners{project_model_face(vertices, view)};
  if(!corners || corners->size() != 4) return false;
  if(!rounded_facing(*corners)) return true;
  std::array<vec2<int16_t>, 4> midpoints;
  for(unsigned int i{0}; i != 4; ++i) {
    vec3<int32_t> sum;
    for(unsigned int axis{0}; axis != 3; ++axis) sum[axis] = std::bit_cast<int32_t>(static_cast<uint32_t>(camera[i][axis]) + static_cast<uint32_t>(camera[(i + 1) % 4][axis]));
    int32_t const depth{sum.z >> 16};
    if(!depth) return false;                                                   // The original dword IDIV does not complete.
    for(unsigned int axis{0}; axis != 2; ++axis) {
      // BC89F retains its normal-aperture shift even in missile-nose view;
      // unlike B9434, its two immediates are not patched by the wide setter.
      unsigned int constexpr midpoint_projection_shift{8};
      int32_t const coordinate{(sum[axis] >> midpoint_projection_shift) / depth};
      midpoints[i][axis] = math::signed_word(static_cast<uint16_t>(coordinate + static_cast<int32_t>(view.projection_origin[axis])));
    }
  }
  pattern_polygon(output, seed, integer_polygon_spans(midpoints, view.viewport_minimum, view.viewport_maximum), colour, mode);
  for(unsigned int i{0}; i != 4; ++i) {
    auto const curve{rounded_arc<2>(vec2<int32_t>{midpoints[(i + 3) % 4]}, vec2<int32_t>{(*corners)[i]}, vec2<int32_t>{midpoints[i]})};
    std::array<vec2<int16_t>, 8> points;
    for(unsigned int j{0}; j != points.size(); ++j) points[j] = vec2<int16_t>{curve[j]};
    // BC78E temporarily bypasses BD68C's winding test for each corner fan.
    pattern_polygon(output, seed, integer_polygon_spans(points, view.viewport_minimum, view.viewport_maximum, polygon_facing::unchecked), colour, mode);
  }
  return true;
}

} // namespace tfx::render
