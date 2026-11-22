#include "map_view.h"
#include <algorithm>
#include <utility>
#include "presentation/drawing.h"
#include "resources/media.h"

namespace tfx::planning {

map_view::map_view(resources::archive const &game, resources::planner_artwork const &artwork,
  presentation::widget_font const &font, document const &plan)
  : artwork{artwork},
    font{font},
    plan{plan} {
  // Native 0xabd64 orders the authored map resources as physical, AWAC, political.
  std::array<unsigned int, 3> constexpr source_indices{1, 2, 0};
  for(unsigned int index{0}; index != layers.size(); ++index) {
    auto const source{game.read("iff/raw/" + plan.map_images[source_indices[index]])};
    layers[index].image = resources::decode_screen(source).image;
    layers[index].colours = artwork.colours(std::span{source}.subspan(4, 256 * 3));
  }
  draw();
}
void map_view::select(map_layer const layer) {
  layers.at(std::to_underlying(layer));
  selected = layer;
  draw();
}
void map_view::draw() {
  unsigned int constexpr map_pixels{200};
  display.pixels.fill(0);
  auto const &layer{layers[std::to_underlying(selected)]};
  for(unsigned int row{0}; row != map_pixels; ++row) std::ranges::copy_n(layer.image.pixels.begin() + row * render::screen_width, map_pixels, display.pixels.begin() + row * render::screen_width);
  artwork.backdrop().paint(display);
  auto const &title{artwork.region(plan.world)};
  int const x{259 - font.measure(title) / 2};
  font.draw(display, {x + 1, 8}, title, 240);
  font.draw(display, {x, 7}, title, 252);
}
void map_view::route(render::framebuffer &target, unsigned int const count) const {
  if(plan.routes.empty()) return;
  auto const &points{plan.routes.front().points};
  auto const position{[](waypoint const &point){return vec2<int>{point.cell.x, 199 - point.cell.y};}};
  auto const label{[&](waypoint const &point){
    auto const at{position(point)};
    if(at.x < 4 || at.y < 4 || at.x > 195 || at.y > 195) return;
    target.pixels[at.y * render::screen_width + at.x] = 255;
    auto const text{std::to_string(point.ordinal)};
    int const width{font.measure(text) + 1};
    int x{at.x - (width - 1) / 2};
    if(x < 4) {
      x = at.x + 1;
    } else if(x + width - 1 > 195) x = at.x - width;
    int const y{at.y - 7 >= 4 ? at.y - 7 : at.y + 1};
    for(int row{y}; row < y + 7; ++row) {
      for(int column{x}; column < x + width; ++column) target.pixels[row * render::screen_width + column] = 243;
    }
    font.draw(target, {x + 1, y + 1}, text, 248);
  }};
  unsigned int const end{std::min(count, static_cast<unsigned int>(points.size()))};
  for(unsigned int index{1}; index < end; ++index) {
    auto const from{position(points[index - 1])};
    auto const to{position(points[index])};
    presentation::draw_clipped_line(target, vec2<int>{from}, vec2<int>{to}, {4, 4}, {195, 195}, 255);
    label(points[index - 1]);
  }
  if(end) label(points[end - 1]);
}
unsigned int map_view::extend(render::framebuffer &target, unsigned int const count, unsigned int const extent) const {
  if(plan.routes.empty() || !count || count >= plan.routes.front().points.size()) return 0;
  auto const &points{plan.routes.front().points};
  vec2<int> const from{points[count - 1].cell.x, 199 - points[count - 1].cell.y};
  vec2<int> const to{points[count].cell.x, 199 - points[count].cell.y};
  return presentation::draw_partial_line(target, vec2<int>{from}, vec2<int>{to}, {4, 4}, {195, 195}, 243, extent);
}
render::framebuffer const &map_view::image() const {
  return display;
}
std::array<uint8_t, 256 * 3> const &map_view::palette() const {
  return layers[std::to_underlying(selected)].colours;
}

} // namespace tfx::planning
