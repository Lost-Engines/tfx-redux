#include "model_raster.h"
#include "model_projection.h"
#include "near_clip.h"
#include "pattern_raster.h"
#include "presentation/drawing.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace tfx::render {
namespace {

int constexpr circle_radius_limit{100};
// The original 4,950 row extents are precisely midpoint-circle octants.
// Generate their integer construction rather than embedding executable data.
constexpr auto circle_rows{[]{
  std::array<std::array<uint8_t,circle_radius_limit>,circle_radius_limit> rows{};
  for(int radius{1}; radius != circle_radius_limit; ++radius) {
    int x{0},y{radius},decision{1 - radius};
    auto &row{rows[static_cast<unsigned int>(radius)]};
    while(x <= y) {
      if(y) row[static_cast<unsigned int>(radius - y)] = std::max(row[static_cast<unsigned int>(radius - y)],static_cast<uint8_t>(x));
      if(x) row[static_cast<unsigned int>(radius - x)] = std::max(row[static_cast<unsigned int>(radius - x)],static_cast<uint8_t>(y));
      ++x;
      if(decision < 0) decision += 2 * x + 1;
      else { --y;decision += 2 * (x - y) + 1; }
    }
  }
  return rows;
}()};

void circle(framebuffer &output, vec2<int16_t> const &point, int const radius, camera const &view, uint8_t const ink, int const mode, uint16_t &seed) {
  if(radius < 1 || radius >= circle_radius_limit) return;
  std::vector<polygon_span> spans;
  spans.reserve(static_cast<unsigned int>(radius * 2 + 1));
  auto const &rows{circle_rows[static_cast<unsigned int>(radius)]};
  int const first{std::max(static_cast<int>(view.viewport_minimum.y),point.y - radius)};
  int const last{std::min(static_cast<int>(view.viewport_maximum.y),point.y + radius)};
  int bottom{point.y + radius};
  for(int y{first}; y <= last; ++y) {
    int const distance{std::abs(y - point.y)};
    int const width{distance ? rows[static_cast<unsigned int>(radius - distance)] : radius};
    int const left{std::max(static_cast<int>(view.viewport_minimum.x),point.x - width)};
    int const right{std::min(static_cast<int>(view.viewport_maximum.x),point.x + width)};
    if(left <= right) spans.push_back({
      .row{static_cast<int16_t>(y)},
      .left{static_cast<int16_t>(left)},
      .right{static_cast<int16_t>(right)},
    });
    else if(y >= point.y) --bottom;
  }
  // Native clipping omits horizontally empty rows from its stack and only
  // decrements the lower endpoint for omissions at/after the centre. Clamping
  // that endpoint afterwards can shift a disc crossing two viewport edges.
  bottom = std::min(bottom,static_cast<int>(view.viewport_maximum.y));
  for(unsigned int i{0}; i != spans.size(); ++i) spans[i].row = static_cast<int16_t>(bottom - static_cast<int>(spans.size() - 1 - i));
  pattern_polygon(output,seed,spans,ink,mode);
}

} // namespace

bool paint_model_primitive(framebuffer &output, models::primitive const &source, std::span<models::model_vertex const> const vertices, camera const &view, std::span<uint8_t const> const texture, colour_map const *const remap, uint16_t &seed) {
  if(paint_model_polygon(output,source,vertices,view,texture,remap,seed)) return true;
  if(source.kind == models::primitive_kind::line && vertices.size() == 2 && view.projection == projection_mode::original_model) {
    auto a{model_camera_point(vertices[0])}, b{model_camera_point(vertices[1])};
    if(!a || !b) return false;
    bool const behind_a{a->z >> 16 <= model_near_depth}, behind_b{b->z >> 16 <= model_near_depth};
    if(behind_a && behind_b) return true;
    vec2<int16_t> const origin{static_cast<int16_t>(view.projection_origin.x),static_cast<int16_t>(view.projection_origin.y)};
    auto const from{behind_a ? std::optional{project_near_plane(intersect_near_plane(*a,*b),view.opening,origin)} : project_model_point(*a,view)};
    auto const to{behind_b ? std::optional{project_near_plane(intersect_near_plane(*b,*a),view.opening,origin)} : project_model_point(*b,view)};
    if(!from || !to) return false;
    uint8_t const index{static_cast<uint8_t>(source.colour)};
    presentation::draw_clipped_line(output,vec2<int>{*from},vec2<int>{*to},vec2<int>{view.viewport_minimum},vec2<int>{view.viewport_maximum},remap ? (*remap)[index] : index);
    return true;
  }
  if(source.kind != models::primitive_kind::point || vertices.size() != 1 || view.projection != projection_mode::original_model) return false;
  auto const raw{model_camera_point(vertices.front())};
  if(!raw) return false;
  int32_t const depth{raw->z >> 16};
  if(depth <= model_near_depth) return true;
  auto const point{project_model_point(*raw,view)};
  if(!point) return false;
  uint8_t const index{static_cast<uint8_t>(source.colour)};
  uint8_t const ink{remap ? (*remap)[index] : index};
  if(source.pixel_size) {
    bool const inside{source.pixel_size == 1
      ? point->x >= view.viewport_minimum.x && point->x <= view.viewport_maximum.x && point->y >= view.viewport_minimum.y && point->y <= view.viewport_maximum.y
      : point->x > view.viewport_minimum.x && point->x < view.viewport_maximum.x && point->y > view.viewport_minimum.y && point->y < view.viewport_maximum.y};
    if(!inside) return true;
    for(unsigned int y{0}; y != source.pixel_size; ++y) for(unsigned int x{0}; x != source.pixel_size; ++x) {
      int const at{(point->y + static_cast<int>(y)) * screen_width + point->x + static_cast<int>(x)};
      if(at >= 0 && at < static_cast<int>(output.pixels.size())) output.write(static_cast<unsigned int>(at),ink);
    }
    return true;
  }
  double const radius_word{std::nearbyint(source.radius * 2 / vertices.front().projection_scale)};
  if(!std::isfinite(radius_word) || radius_word < 0 || radius_word > std::numeric_limits<uint16_t>::max()) return false;
  // Radius uses signed WORD * 128 / high-depth in both apertures. Only the
  // centre projection changes for the wider missile-nose view.
  int const radius{math::signed_word(static_cast<uint16_t>(radius_word)) * 128 / depth};
  if(radius < std::numeric_limits<int16_t>::min() || radius > std::numeric_limits<int16_t>::max()) return false;
  circle(output,*point,radius,view,ink,source.mode,seed);
  return true;
}

} // namespace tfx::render
