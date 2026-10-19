#include "integer_polygon.h"
#include "integer_texture_polygon.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <vector>
#include <utility>

namespace tfx::render {
namespace {
int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }
struct raster_vertex {
  vec2<int16_t> position;
  uint16_t shade_q8{0};
  vec2<uint16_t> uv_q7{0,0};
};
using vertex_list = std::vector<raster_vertex>;
int reciprocal(int const height) { return height == 1 ? 32767 : 32768 / height; }
int32_t edge_initial(uint16_t const start, int32_t const step, int const difference, int const height) {
  return wrap((static_cast<uint32_t>(start) << 16) + 32768u + (difference >= height ? static_cast<uint32_t>(step >> 1) : 0u));
}

vertex_list clip(vertex_list const &vertices, unsigned int const axis, int16_t const boundary, int const sign) {
  vertex_list result;
  for(unsigned int i{0}; i != vertices.size(); ++i) {
    auto const &a{vertices[i]}, &b{vertices[(i + 1) % vertices.size()]};
    int const da{word(a.position[axis] - boundary) * sign}, db{word(b.position[axis] - boundary) * sign};
    bool const inside{da >= 0};
    if(inside) result.emplace_back(a);
    if(inside == (db >= 0)) continue;
    int const aa{std::abs(da)}, ab{std::abs(db)};
    int const delta{word(b.position[1 - axis] - a.position[1 - axis])};
    // Native clippers calculate from the farther endpoint; equality chooses
    // the inside endpoint. This retains their directional IDIV rounding.
    int const other{aa > ab || (aa == ab && inside) ? a.position[1 - axis] + delta * aa / (aa + ab) : b.position[1 - axis] - delta * ab / (aa + ab)};
    vec2<int16_t> point;
    point[axis] = boundary;point[1 - axis] = word(other);
    int const shade_delta{word(b.shade_q8 - a.shade_q8)};
    int const shade{aa > ab || (aa == ab && inside) ? a.shade_q8 + shade_delta * aa / (aa + ab) : b.shade_q8 - shade_delta * ab / (aa + ab)};
    vec2<uint16_t> uv;
    for(unsigned int component{0}; component != 2; ++component) {
      int const difference{word(b.uv_q7[component] - a.uv_q7[component])};
      int const value{aa > ab || (aa == ab && inside) ? a.uv_q7[component] + difference * aa / (aa + ab) : b.uv_q7[component] - difference * ab / (aa + ab)};
      uv[component] = static_cast<uint16_t>(value);
    }
    result.push_back({
      .position{vec2<int16_t>{point}},
      .shade_q8{static_cast<uint16_t>(shade)},
      .uv_q7{vec2<uint16_t>{uv}},
    });
  }
  return result;
}
struct edge {
  unsigned int rows;
  int32_t position_q16;
  int32_t step_q16;
  int32_t shade_q16;
  int32_t shade_step_q16;
  vec2<int32_t> uv_accumulator;
  vec2<int32_t> uv_step;
};
struct edge_chain {
  std::vector<edge> segments;
  int16_t final_x;
  uint16_t final_shade;
  vec2<uint16_t> final_uv;
};
std::optional<edge_chain> chain(vertex_list const &vertices, unsigned int index, int const direction, int16_t const bottom) {
  edge_chain result;
  for(unsigned int count{0}; count <= vertices.size(); ++count) {
    auto const &a{vertices[index]};
    if(count && a.position.y >= bottom) { result.final_x = a.position.x;result.final_shade = a.shade_q8;result.final_uv = a.uv_q7;return result; }
    unsigned int const count_vertices{static_cast<unsigned int>(vertices.size())};
    index = (index + count_vertices + direction) % count_vertices;
    auto const &b{vertices[index]};
    int const height{b.position.y - a.position.y}, dx{b.position.x - a.position.x};
    if(height < 0) return std::nullopt;
    if(!height) continue;
    int32_t const step{dx * 2 * reciprocal(height)};
    int const shade_delta{word(b.shade_q8 - a.shade_q8)};
    int32_t const shade_step{shade_delta * 2 * reciprocal(height)};
    vec2<int32_t> uv_start, uv_step;
    for(unsigned int component{0}; component != 2; ++component) {
      int32_t const difference{static_cast<int32_t>(b.uv_q7[component]) - a.uv_q7[component]};
      uv_step[component] = wrap(static_cast<uint32_t>(difference) * 2u * static_cast<uint32_t>(reciprocal(height)));
      // DB3C8/DB4DC always add half a texture step at an edge's first row.
      uv_start[component] = wrap((static_cast<uint32_t>(a.uv_q7[component]) << 16) + 32768u + static_cast<uint32_t>(uv_step[component] >> 1));
    }
    result.segments.push_back({
      .rows{static_cast<unsigned int>(height)},
      .position_q16{edge_initial(static_cast<uint16_t>(a.position.x),step,std::abs(dx),height)},
      .step_q16{step},
      .shade_q16{edge_initial(a.shade_q8,shade_step,word(std::abs(shade_delta)),height)},
      .shade_step_q16{shade_step},
      .uv_accumulator{vec2<int32_t>{uv_start}},
      .uv_step{vec2<int32_t>{uv_step}},
    });
  }
  return std::nullopt;
}
class edge_walker {
private:
  edge_chain const &source;
  unsigned int index{0}, row{0};
  int32_t position{0}, shade_position{0};
  vec2<int32_t> uv_position;
  void start() {
    if(index == source.segments.size()) return;
    position = source.segments[index].position_q16;
    shade_position = source.segments[index].shade_q16;
    uv_position = source.segments[index].uv_accumulator;
  }
public:
  explicit edge_walker(edge_chain const &chain) : source{chain} {
    start();
  }
  int x() const { return index == source.segments.size() ? source.final_x : position >> 16; }
  uint16_t shade() const { return index == source.segments.size() ? source.final_shade : static_cast<uint16_t>(shade_position >> 16); }
  vec2<uint16_t> uv() const { return index == source.segments.size() ? vec2<uint16_t>{source.final_uv} : vec2<uint16_t>{static_cast<uint16_t>(uv_position.x >> 16),static_cast<uint16_t>(uv_position.y >> 16)}; }
  void advance() {
    if(index == source.segments.size()) return;
    if(++row == source.segments[index].rows) {
      ++index;row = 0;
      start();
    } else {
      position = wrap(static_cast<uint32_t>(position) + static_cast<uint32_t>(source.segments[index].step_q16));
      shade_position = wrap(static_cast<uint32_t>(shade_position) + static_cast<uint32_t>(source.segments[index].shade_step_q16));
      for(unsigned int component{0}; component != 2; ++component) uv_position[component] = wrap(static_cast<uint32_t>(uv_position[component]) + static_cast<uint32_t>(source.segments[index].uv_step[component]));
    }
  }
};

class palette_accumulator {
private:
  uint32_t value, step;
  bool const descending;
  bool carry{false};
public:
  palette_accumulator(uint32_t const initial, int32_t const delta, bool const negative) : value{std::rotr(initial >> 8,16)}, step{std::rotr(static_cast<uint32_t>(delta >> 8),16)}, descending{negative} {
    // Native VGA stepping rotates the integer word below its fraction and
    // propagates ADD/ADC or SUB/SBB carry between pixels. Its initial phase
    // advances only the fraction, retaining that first carry for the next pixel.
    uint32_t const integer{value & 65535};
    advance();value = (value & 0xffff0000u) | integer;
  }
  uint8_t colour() const { return static_cast<uint8_t>(value); }
  void advance() {
    if(descending) {
      uint64_t const amount{static_cast<uint64_t>(step) + carry};
      bool const borrow{static_cast<uint64_t>(value) < amount};
      value = static_cast<uint32_t>(static_cast<uint64_t>(value) - amount);carry = borrow;
    } else {
      uint64_t const sum{static_cast<uint64_t>(value) + step + carry};
      value = static_cast<uint32_t>(sum);carry = sum >> 32;
    }
  }
};

void shaded_span(framebuffer &output, int const y, int const left, int const right, uint16_t const left_shade, uint16_t const right_shade, colour_map const &remap) {
  int const extent{right - left};
  if(extent < 0) return;
  int const delta{word(right_shade - left_shade)}, sign{delta < 0 ? -1 : 1}, magnitude{word(std::abs(delta))};
  int32_t const product{word(magnitude * 2) * reciprocal(extent + 1)};
  uint32_t const bias{magnitude >= extent ? static_cast<uint32_t>(sign * (product >> 1)) : 0u};
  palette_accumulator colour{(static_cast<uint32_t>(left_shade) << 16) + 32768u + bias,product,sign < 0};
  for(int x{left}; x <= right; ++x) {
    if(x >= 0 && x < screen_width) output.write(static_cast<unsigned int>(y * screen_width + x),remap[colour.colour()]);
    colour.advance();
  }
}

template<typename Painter>
void raster(vertex_list vertices, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum, Painter const &paint, polygon_facing const facing = polygon_facing::cull) {
  if(vertices.size() < 3) return;
  int x_low{32767},x_high{-32768},y_low{32767},y_high{-32768};
  for(auto const &v : vertices) {
    x_low = std::min(x_low,static_cast<int>(v.position.x));x_high = std::max(x_high,static_cast<int>(v.position.x));
    y_low = std::min(y_low,static_cast<int>(v.position.y));y_high = std::max(y_high,static_cast<int>(v.position.y));
  }
  if(x_high < minimum.x || x_low > maximum.x || y_high < minimum.y || y_low > maximum.y) return;
  int32_t const x1{word(vertices[1].position.x - vertices[0].position.x)}, x2{word(vertices[2].position.x - vertices[0].position.x)};
  int32_t const y1{word(vertices[1].position.y - vertices[0].position.y)}, y2{word(vertices[2].position.y - vertices[0].position.y)};
  if(facing == polygon_facing::cull && wrap(static_cast<uint32_t>(x1 * y2) - static_cast<uint32_t>(x2 * y1)) < 0) return;
  // Native bounding-span overflow clamps all vertices before clipping.
  if(x_high - x_low > 32767 || y_high - y_low > 32767) for(auto &v : vertices) {
    v.position.x = std::clamp<int16_t>(v.position.x,-16383,16383);v.position.y = std::clamp<int16_t>(v.position.y,-16383,16383);
  }
  vertices = clip(vertices,1,maximum.y,-1);
  vertices = clip(vertices,1,minimum.y,1);
  vertices = clip(vertices,0,maximum.x,-1);
  vertices = clip(vertices,0,minimum.x,1);
  if(vertices.size() < 3) return;
  unsigned int top{0};int16_t bottom{vertices.front().position.y};
  for(unsigned int i{1}; i != vertices.size(); ++i) {
    if(vertices[i].position.y <= vertices[top].position.y) top = i;
    bottom = std::max(bottom,vertices[i].position.y);
  }
  auto const left{chain(vertices,top,-1,bottom)}, right{chain(vertices,top,1,bottom)};
  if(!left || !right) return;
  edge_walker l{*left},r{*right};
  for(int y{vertices[top].position.y}; y <= bottom; ++y) {
    if(y >= 0 && y < screen_height) paint(y,l,r);
    l.advance();r.advance();
  }
}

} // anonymous namespace

void integer_polygon(framebuffer &output, std::span<vec2<int16_t> const> const input, uint8_t const colour, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum) {
  auto const spans{integer_polygon_spans(input,minimum,maximum)};
  // The original consumes bottom-first spans and terminates the whole face
  // through its unrolled VGA jump table. The first negative byte-width entry
  // skips a row; wider inverted spans terminate the rest of the face.
  for(auto const &span : spans | std::views::reverse) {
    if((span.right | 3) + 4 < span.left) return;
    int const first{std::max(0,static_cast<int>(span.left))},last{std::min(screen_width - 1,static_cast<int>(span.right))};
    if(first <= last) std::fill_n(output.pixels.begin() + span.row * screen_width + first,last - first + 1,colour);
  }
}
std::vector<polygon_span> integer_polygon_spans(std::span<vec2<int16_t> const> const input, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum, polygon_facing const facing) {
  vertex_list vertices;vertices.reserve(input.size());
  for(auto const &point : input) vertices.push_back({
    .position{vec2<int16_t>{point}},
    .shade_q8{0},
  });
  std::vector<polygon_span> result;
  raster(std::move(vertices),minimum,maximum,[&](int const y, edge_walker const &left, edge_walker const &right) {
    result.push_back({
      .row{static_cast<int16_t>(y)},
      .left{static_cast<int16_t>(left.x())},
      .right{static_cast<int16_t>(right.x())},
    });
  },facing);
  return result;
}
void integer_gouraud_polygon(framebuffer &output, std::span<indexed_vertex const> const vertices, colour_map const &remap, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum) {
  vertex_list source;source.reserve(vertices.size());
  for(auto const &vertex : vertices) source.push_back({
    .position{vec2<int16_t>{vertex.position}},
    .shade_q8{vertex.shade_q8},
  });
  struct shaded_scanline {
    int row;
    int left;
    int right;
    uint16_t left_shade;
    uint16_t right_shade;
  };
  std::vector<shaded_scanline> spans;spans.reserve(screen_height);
  raster(std::move(source),minimum,maximum,[&](int const y, edge_walker const &left, edge_walker const &right) {
    spans.push_back({
      .row{y},
      .left{left.x()},
      .right{right.x()},
      .left_shade{left.shade()},
      .right_shade{right.shade()},
    });
  });
  int constexpr vga_plane_mask{3};
  for(auto const &span : spans | std::views::reverse) {
    // The native Gouraud painter tests its four-pixel VGA group before the
    // individual plane masks. An empty span within that group skips a row;
    // only a crossed group ends the polygon. Thin faces can widen again above.
    if((span.right | vga_plane_mask) < span.left) return;
    shaded_span(output,span.row,span.left,span.right,span.left_shade,span.right_shade,remap);
  }
}

void integer_texture_polygon(framebuffer &output, std::span<textured_vertex const> const vertices, std::span<uint8_t const> const texture, colour_map const &remap, vec2<int16_t> const &minimum, vec2<int16_t> const &maximum) {
  vertex_list source;source.reserve(vertices.size());
  for(auto const &vertex : vertices) source.push_back({
    .position{vec2<int16_t>{vertex.position}},
    .uv_q7{vec2<uint16_t>{vertex.uv_q7}},
  });
  struct texture_span {
    int row;
    int left;
    int right;
    vec2<uint16_t> left_uv_q7;
    vec2<uint16_t> right_uv_q7;
  };
  std::vector<texture_span> spans;spans.reserve(screen_height);
  raster(std::move(source),minimum,maximum,[&](int const y, edge_walker const &left, edge_walker const &right) {
    spans.push_back({
      .row{y},
      .left{left.x()},
      .right{right.x()},
      .left_uv_q7{left.uv()},
      .right_uv_q7{right.uv()},
    });
  });
  // DBFA0 pushes top-to-bottom spans; the VGA consumer pops the bottom first.
  for(auto const &span : spans | std::views::reverse) {
    if((span.right | 3) < span.left) return;
    int const width{span.right - span.left + 1};
    if(width <= 0) continue;
    auto const &first{span.left_uv_q7}, &last{span.right_uv_q7};
    vec2<uint32_t> coordinate, step;
    for(unsigned int component{0}; component != 2; ++component) {
      int32_t const difference{static_cast<int32_t>(last[component]) - first[component]};
      step[component] = static_cast<uint32_t>(difference) * 4u * static_cast<uint32_t>(reciprocal(width));
      coordinate[component] = (static_cast<uint32_t>(first[component]) << 17) + 65536u + static_cast<uint32_t>(wrap(step[component]) >> 1);
    }
    for(int x{span.left}; x <= span.right; ++x) {
      unsigned int const index{((coordinate.y >> 24) << 8) | (coordinate.x >> 24)};
      uint8_t const ink{remap[index < texture.size() ? texture[index] : uint8_t{0}]};
      if(ink && x >= 0 && x < screen_width) output.write(static_cast<unsigned int>(span.row * screen_width + x),ink);
      coordinate.x += step.x;
      // The original VGA loop holds V in EAX and overwrites AL with the
      // remapped texel before ADD. Retain that otherwise tiny fraction change.
      coordinate.y = ((coordinate.y & 0xffffff00u) | ink) + step.y;
    }
  }
}

} // namespace tfx::render
