#include "software_renderer.h"
#include "point_size.h"
#include "cockpit/vector_hud.h"
#include "model_raster.h"
#include "rounded_curve.h"
#include "integer_polygon.h"
#include "pattern_raster.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace tfx::render {
namespace {

models::model_vertex interpolate(models::model_vertex const &a, models::model_vertex const &b, double const t) {
  return {
    .position{a.position + (b.position - a.position) * t},
    .uv{a.uv + (b.uv - a.uv) * t},
    .shade{a.shade + (b.shade - a.shade) * t},
    .projection_scale{a.projection_scale + (b.projection_scale - a.projection_scale) * t},
  };
}

} // anonymous namespace

std::array<vec2<int32_t>, 8> rounded_corner(vec2<int32_t> const &previous, vec2<int32_t> const &corner, vec2<int32_t> const &next) {
  return rounded_arc<2>(previous,corner,next);
}

software_renderer::software_renderer(resources::executable_tables const &source_tables, uint16_t const seed) : tables{source_tables}, procedural_seed{seed} {
}

void software_renderer::ink(int const x, int const y, uint8_t const colour, int const mode) {
  if(x < clipping.glyph_minimum.x || x >= clipping.glyph_limit.x || y < clipping.glyph_minimum.y || y >= clipping.glyph_limit.y) return;
  pattern_pixel(output,x,y,colour,mode);
}

uint16_t software_renderer::procedural_state() const {
  return procedural_seed;
}

void software_renderer::patterned_span(int const y, double const left, double const right, uint8_t const colour, int const mode, int const bottom, bool const inclusive, bool const paint) {
  pattern_span(output,procedural_seed,y,left,right,colour,mode,bottom,inclusive,paint);
}

void software_renderer::background(camera const &view, uint8_t const sky, uint8_t const ground) {
  /// Independent free-inspection backdrop; live flight uses the original banded horizon
  for(int y{0}; y != output.size.y; ++y) {
    auto const ray_y{view.forward.y + (y + .5 - view.projection_origin.y) / view.focal_distance * view.down.y};
    auto const colour{ray_y > 0 ? ground : sky};
    std::fill_n(output.pixels.begin() + y * output.size.x, output.size.x, colour);
  }
}

void software_renderer::polygon(std::span<screen_vertex const> const vertices, uint8_t const colour, int const mode, std::span<uint8_t const> const texture, colour_map const *const remap, std::span<uint16_t const> const overlay) {
  /// Half-open software spans; interpolate palette indices and UVs in screen space, preserving authored paint order
  if(vertices.size() < 3) return;
  bool const shaded{texture.empty() && std::ranges::all_of(vertices,[](auto const &v){ return v.shade >= 0; })};
  double top{static_cast<double>(output.size.y)}, bottom{0};
  for(auto const &v : vertices) { top = std::min(top, v.position.y); bottom = std::max(bottom, v.position.y); }
  int const first{std::max({0,clipping.glyph_minimum.y,static_cast<int>(std::ceil(top - .5))})}, last{std::min({output.size.y,clipping.glyph_limit.y,static_cast<int>(std::ceil(bottom - .5))})};
  auto &hits{scanline_hits};
  hits.reserve(vertices.size());
  int const bottom_row{static_cast<int>(std::lround(bottom))}, top_row{static_cast<int>(std::lround(top))};
  for(int y{mode == 4 ? bottom_row : last - 1}; y >= (mode == 4 ? top_row : first); --y) {
    double const sample{mode == 4 ? (y == bottom_row ? y - 1e-9 : y) : shaded ? y : y + .5};
    hits.clear();
    for(size_t i{0}; i != vertices.size(); ++i) {
      auto const *a{&vertices[i]}, *b{&vertices[(i + 1) % vertices.size()]};
      if(a->position.y > b->position.y) std::swap(a,b);
      if(sample < a->position.y || sample >= b->position.y) continue;
      auto const t{(sample - a->position.y) / (b->position.y - a->position.y)};
      double shade{a->shade + (b->shade - a->shade) * t};
      if(shaded) {
        double const height{b->position.y - a->position.y};
        int const first_colour{static_cast<int>(a->shade * 256)};
        int const difference{static_cast<int>(b->shade * 256) - first_colour};
        double const step{difference * 2 * std::floor(32768 / std::max(1.0,height)) / 65536};
        shade = std::floor(first_colour + .5 + (std::abs(difference) >= height ? step / 2 : 0) + step * (y - a->position.y)) / 256;
      }
      hits.push_back({
        .position{a->position + (b->position - a->position) * t},
        .uv{a->uv + (b->uv - a->uv) * t},
        .shade{shade},
      });
    }
    std::ranges::sort(hits, {}, [](auto const &v){ return v.position.x; });
    for(size_t pair{0}; pair + 1 < hits.size(); pair += 2) {
      auto const &left{hits[pair]}, &right{hits[pair + 1]};
      int const start{std::max({0,clipping.glyph_minimum.x,static_cast<int>(std::ceil(left.position.x - .5))})}, end{std::min({output.size.x,clipping.glyph_limit.x,static_cast<int>(std::ceil(right.position.x - .5))})};
      if(mode >= 0 && texture.empty() && left.shade < 0) {
        patterned_span(y,left.position.x,right.position.x,colour,mode,bottom_row,false,y >= first && y < last);
        continue;
      }
      if(start >= end) continue;
      int const colour_origin{static_cast<int>(std::ceil(left.position.x))};
      double colour_start{0}, colour_step{0};
      if(shaded) {
        int const extent{std::max(1,static_cast<int>(std::floor(right.position.x)) - colour_origin)};
        int const first_colour{static_cast<int>(left.shade * 256)}, last_colour{static_cast<int>(right.shade * 256)};
        int const difference{math::signed_word(static_cast<uint16_t>(last_colour - first_colour))}, sign{difference < 0 ? -1 : 1};
        double const product{std::abs(difference) * 2 * std::floor(32768.0 / (extent + 1))};
        colour_step = sign * std::floor(product / 256) / 65536;
        double const bias{std::floor((32768 + (std::abs(difference) >= extent ? sign * std::floor(product / 2) : 0)) / 256) / 65536};
        colour_start = first_colour / 256.0 + bias;
      }
      double const inverse_width{1 / (right.position.x - left.position.x)};
      if(!texture.empty() && mode == -1) {
        // These spans are already clipped. Avoid two out-of-line pixel calls,
        // repeated bounds/mode checks and optional coverage work per texel.
        auto const paint{[&]<bool mapped, bool live, bool covered>() {
          double const du{right.uv.x-left.uv.x}, dv{right.uv.y-left.uv.y};
          auto *destination{output.pixels.data() + y * output.size.x};
          for(int x=start; x<end; ++x) {
            double const t{(x + .5 - left.position.x) * inverse_width};
            int const u{std::clamp(static_cast<int>(left.uv.x + du*t),0,255)};
            int const v{std::clamp(static_cast<int>(left.uv.y + dv*t),0,191)};
            auto const at{static_cast<unsigned int>(v*256+u)};
            uint8_t index{texture[at]};
            // Transparent texels must stay holes when distance shading maps
            // black into the reserved UI colours. Live screens can supply
            // explicitly opaque black over those same source texels.
            if constexpr(live) { if(!index && !overlay[at]) continue; }
            else if(!index) continue;
            if constexpr(mapped) index=(*remap)[index];
            if constexpr(live) {
              if(overlay[at]) index=static_cast<uint8_t>(overlay[at]);
              else if(!index) continue;
            } else if(!index) continue;
            destination[x]=index;
            if constexpr(covered) output.coverage[static_cast<size_t>(y*output.size.x+x)]=255;
          }
        }};
        auto const dispatch{[&]<bool mapped, bool live>() {
          if(output.coverage.empty()) paint.template operator()<mapped,live,false>();
          else paint.template operator()<mapped,live,true>();
        }};
        if(overlay.empty()) {
          if(remap) dispatch.template operator()<true,false>();else dispatch.template operator()<false,false>();
        } else {
          if(remap) dispatch.template operator()<true,true>();else dispatch.template operator()<false,true>();
        }
        continue;
      }
      for(int x{start}; x != end; ++x) {
        auto const t{(x + .5 - left.position.x) * inverse_width};
        uint8_t index{colour};
        uint16_t replacement{0};
        if(!texture.empty()) {
          auto const uv{left.uv + (right.uv - left.uv) * t};
          int const u{std::clamp(static_cast<int>(uv.x),0,255)}, v{std::clamp(static_cast<int>(uv.y),0,191)};
          auto const at{static_cast<unsigned int>(v * 256 + u)};
          index = texture[at];
          if(!overlay.empty()) replacement=overlay[at];
          if(!index && !replacement) continue;
        } else if(shaded) index = static_cast<uint8_t>(static_cast<int>(std::floor(colour_start + colour_step * (x - colour_origin))));
        if(remap && (!texture.empty() || shaded)) index = (*remap)[index];
        // Original texture transparency is tested after its lighting lookup.
        if(replacement) index=static_cast<uint8_t>(replacement);
        else if(!texture.empty() && !index) continue;
        ink(x,y,index,mode);
      }
    }
  }
}

void software_renderer::circle(int const x, int const y, unsigned int const radius, uint8_t const colour, int const mode) {
  /// Use the actual native integer span table; the original rejects radii outside 1..99
  if(output.size != vec2<int>{screen_width,screen_height} || radius >= 100) {
    int const r{static_cast<int>(std::min<unsigned int>(radius,static_cast<unsigned int>(std::max(output.size.x,output.size.y)) * 2u))};
    for(int row{std::max(-r,-y)}; row <= std::min(r,output.size.y - 1 - y); ++row) {
      int const extent{static_cast<int>(std::sqrt(static_cast<double>(r) * r - static_cast<double>(row) * row))};
      patterned_span(y + row,x - extent,x + extent,colour,mode,y + r,true);
    }
    return;
  }
  auto const table{tables.circle(radius)};
  if(table.empty()) return;
  auto const r{static_cast<int>(radius)}, bottom{y + r};
  for(int row{bottom}; row >= y - r; --row) {
    int const d{std::abs(row - y)}, extent{d == 0 ? r : table[static_cast<unsigned int>(r - d)]};
    patterned_span(row,x - extent,x + extent,colour,mode,bottom,true);
  }
}

void software_renderer::begin_points() {
  collect_points = true;points.clear();point_depth.begin(output.size);
}
void software_renderer::finish_points() {
  collect_points = false;
  std::stable_sort(points.begin(),points.end(),[](auto const &a, auto const &b) { return a.depth>b.depth; });
  for(auto const &point : points) if(point_depth.visible(point.ray,point.depth)) point_disc(point);
  points.clear();
}
void software_renderer::point_disc(point_sprite const &point) {
  int const extent{std::max(1,static_cast<int>(std::lround(point.diameter)))};
  if(extent==1) { ink(point.position.x,point.position.y,point.colour,point.mode);return; }
  double const offset{extent%2 ? 0 : .5}, radius{(extent-.5)/2};
  for(int y{-extent/2}; y<=extent/2; ++y) for(int x{-extent/2}; x<=extent/2; ++x)
    if((x-offset)*(x-offset)+(y-offset)*(y-offset)<=radius*radius)
      ink(point.position.x+x,point.position.y+y,point.colour,point.mode);
}

void software_renderer::line(vec2d const &a, vec2d const &b, uint8_t const colour) {
  /// Integer Bresenham segments after camera clipping
  int x{static_cast<int>(std::lround(a.x))}, y{static_cast<int>(std::lround(a.y))};
  int const end_x{static_cast<int>(std::lround(b.x))}, end_y{static_cast<int>(std::lround(b.y))};
  int const dx{std::abs(end_x - x)}, dy{std::abs(end_y - y)}, sx{x < end_x ? 1 : -1}, sy{y < end_y ? 1 : -1};
  int error{dx - dy};
  for(;;) {
    ink(x,y,colour,-1);
    if(x == end_x && y == end_y) return;
    int const twice{error * 2};
    if(twice > -dy) { error -= dy; x += sx; }
    if(twice < dx) { error += dx; y += sy; }
  }
}

void software_renderer::primitive(models::primitive const &source, std::span<models::model_vertex const> const camera_vertices, camera const &source_view, std::span<uint8_t const> const texture, colour_map const *const remap, std::span<uint16_t const> const overlay) {
  auto view{source_view};
  if(output.size != vec2<int>{screen_width,screen_height} && view.projection == projection_mode::original_model) {
    double const scale{std::min(output.size.x / static_cast<double>(screen_width),output.size.y / static_cast<double>(screen_height))};
    view.projection_origin = view.projection_origin * scale + vec2d{(output.size.x - screen_width * scale) / 2,(output.size.y - screen_height * scale) / 2};
    view.focal_distance *= scale;
    view.projection = projection_mode::continuous;
  }
  /// Clip a complete authored primitive before projection, including its attributes
  if(camera_vertices.empty()) return;
  uint8_t const source_index{static_cast<uint8_t>(source.colour)};
  uint8_t const ink_index{remap ? (*remap)[source_index] : source_index};
  if(overlay.empty() && paint_model_primitive(output,source,camera_vertices,view,texture,remap,procedural_seed)) return;
  if(source.kind == models::primitive_kind::point) {
    auto const &v{camera_vertices[0].position};
    if(v.z <= view.near_distance * camera_vertices.front().projection_scale || v.z > view.far_distance) return;
    auto const point{view.project(v,camera_vertices.front().projection_scale)};
    if(point.x < -100 || point.x > output.size.x + 100 || point.y < -100 || point.y > output.size.y + 100) return;
    int const x{static_cast<int>(std::lround(point.x))}, y{static_cast<int>(std::lround(point.y))};
    // Physical spheres retain their authored size; pixel primitives have
    // separate light, particle and whole-object sizing rules.
    double const pixel_scale{std::max(1.0,view.focal_distance / 512.0)};
    if(source.pixel_size) {
      if(output.size == vec2<int>{screen_width,screen_height} && source.point_role!=models::point_role::object) {
        for(unsigned int row{0}; row<source.pixel_size; ++row) for(unsigned int col{0}; col<source.pixel_size; ++col) ink(x+static_cast<int>(col),y+static_cast<int>(row),ink_index,source.mode);
      } else {
        point_sprite const sprite{{x,y},vec2d{point},v.z,point_diameter(source,output.size,view.focal_distance,v.length()),ink_index,source.mode};
        if(collect_points) points.push_back(sprite);else point_disc(sprite);
      }
    } else circle(x,y,static_cast<unsigned int>(std::max(source.radius * view.focal_distance / v.z,std::floor(pixel_scale / 2))),ink_index,source.mode);
    return;
  }
  if(source.kind == models::primitive_kind::line && camera_vertices.size() == 2) {
    auto a{camera_vertices[0]}, b{camera_vertices[1]};
    for(unsigned int plane{0}; plane != 6; ++plane) {
      auto const distance{[this, &view, plane](models::model_vertex const &vertex){
        auto const &v{vertex.position};
        switch(plane) {
        case 0: return v.z - view.near_distance * vertex.projection_scale;
        case 1: return view.far_distance - v.z;
        case 2: return v.x + view.projection_origin.x / view.focal_distance * v.z;
        case 3: return -v.x + (output.size.x - view.projection_origin.x) / view.focal_distance * v.z;
        case 4: return v.y + view.projection_origin.y / view.focal_distance * v.z;
        default: return -v.y + (output.size.y - view.projection_origin.y) / view.focal_distance * v.z;
        }
      }};
      double const da{distance(a)}, db{distance(b)};
      if(da < 0 && db < 0) return;
      if(da < 0) a = interpolate(a,b,da / (da - db));
      else if(db < 0) b = interpolate(a,b,da / (da - db));
    }
    line(view.project(a.position,a.projection_scale),view.project(b.position,b.projection_scale),ink_index);
    return;
  }
  auto clipped{camera_vertices};
  unsigned int buffer{0};
  for(unsigned int plane{0}; plane != 6; ++plane) {
    auto const distance{[this, &view, plane](models::model_vertex const &vertex){
      auto const &v{vertex.position};
      switch(plane) {
      case 0: return v.z - view.near_distance * vertex.projection_scale;
      case 1: return view.far_distance - v.z;
      case 2: return v.x + view.projection_origin.x / view.focal_distance * v.z;
      case 3: return -v.x + (output.size.x - view.projection_origin.x) / view.focal_distance * v.z;
      case 4: return v.y + view.projection_origin.y / view.focal_distance * v.z;
      default: return -v.y + (output.size.y - view.projection_origin.y) / view.focal_distance * v.z;
      }
    }};
    bool inside{false}, outside{false};
    for(auto const &vertex : clipped) {
      if(distance(vertex) >= 0) inside = true;
      else outside = true;
    }
    if(!inside) return;
    if(!outside) continue;
    auto &next{clip_vertices[buffer]};next.clear();next.reserve(clipped.size() + 1);
    auto previous{clipped.back()};
    auto previous_distance{distance(previous)};
    for(auto const &current : clipped) {
      auto const current_distance{distance(current)};
      if((previous_distance >= 0) != (current_distance >= 0)) next.push_back(interpolate(previous,current,previous_distance / (previous_distance - current_distance)));
      if(current_distance >= 0) next.push_back(current);
      previous = current;
      previous_distance = current_distance;
    }
    clipped = next;buffer ^= 1;
  }
  if(collect_points && source.kind == models::primitive_kind::polygon && !source.rounded) point_depth.add(clipped,view,source.mode,texture,remap);
  auto &projected{projected_vertices};projected.clear();projected.reserve(clipped.size());
  for(auto const &v : clipped) projected.push_back({
    .position{view.project(v.position,v.projection_scale)},
    .uv{vec2d{v.uv}},
    .shade{v.shade},
  });
  if(source.kind == models::primitive_kind::line) {
    if(projected.size() >= 2) line(projected.front().position,projected.back().position,ink_index);
  } else if(source.rounded && projected.size() == 4) {
    std::array<vec2<int32_t>, 4> corners, midpoints;
    for(unsigned int i{0}; i != 4; ++i) corners[i] = vec2<int32_t>{static_cast<int32_t>(std::lround(projected[i].position.x)),static_cast<int32_t>(std::lround(projected[i].position.y))};
    for(unsigned int i{0}; i != 4; ++i) midpoints[i] = vec2<int32_t>{(corners[i].x + corners[(i + 1) % 4].x) >> 1,(corners[i].y + corners[(i + 1) % 4].y) >> 1};
    std::vector<screen_vertex> centre;
    for(auto const &point : midpoints) centre.push_back({
      .position{vec2d{point}},
      .uv{0,0},
      .shade{-1},
    });
    polygon(centre,ink_index,source.mode);
    for(unsigned int i{0}; i != 4; ++i) {
      std::vector<screen_vertex> curve;
      for(auto const &point : rounded_corner(midpoints[(i + 3) % 4],corners[i],midpoints[i])) curve.push_back({
        .position{vec2d{point}},
        .uv{0,0},
        .shade{-1},
      });
      polygon(curve,ink_index,source.mode);
    }
  } else polygon(projected,ink_index,source.mode,texture,remap,overlay);
}

unsigned int software_renderer::text_width(std::string_view const message) const {
  unsigned int result{0};
  for(unsigned char const character : message) {
    if(!character) break;
    result += tables.glyph_measure(character);
  }
  return result;
}

void software_renderer::text(int x, int const y, std::string_view const message, uint8_t const colour, framebuffer *const coverage) {
  /// Native 0x87154 character mapping and advances, with clipped monochrome glyph writes
  for(unsigned char const character : message) {
    if(!character) break;
    if(character == ' ') { x += 3; continue; }
    auto const *const glyph{tables.glyph(character)};
    if(!glyph) continue;
    if(output.hud_capture) { output.hud_capture->glyph(glyph->rows,{static_cast<double>(x),static_cast<double>(y)},clipping);x+=static_cast<int>(glyph->advance);continue; }
    for(int row{0}; row != 6; ++row) {
      for(int column{0}; column != 8; ++column) {
        if((glyph->rows[static_cast<unsigned int>(row)] & (128 >> column)) && x + column >= clipping.glyph_minimum.x && x + column < clipping.glyph_limit.x && y + row >= clipping.glyph_minimum.y && y + row < clipping.glyph_limit.y) {
          ink(x + column,y + row,colour,-1);
          if(coverage && x + column >= 0 && x + column < output.size.x && y + row >= 0 && y + row < output.size.y) coverage->pixels[static_cast<unsigned int>((y + row) * output.size.x + x + column)] = 255;
        }
      }
    }
    x += static_cast<int>(glyph->advance);
  }
}

} // namespace tfx::render
