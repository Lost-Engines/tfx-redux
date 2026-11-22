#include "dialogue.h"
#include <algorithm>
#include "presentation/drawing.h"

namespace tfx::presentation {
namespace {

void fill(render::framebuffer &target, vec2<int> const &minimum, vec2<int> const &maximum, uint8_t const colour) {
  for(int y{std::max(minimum.y, 0)}; y <= std::min(maximum.y, render::screen_height - 1); ++y) {
    int const left{std::max(minimum.x, 0)};
    int const right{std::min(maximum.x, render::screen_width - 1)};
    if(left <= right) std::fill_n(target.pixels.begin() + y * render::screen_width + left, right - left + 1, colour);
  }
}
void bevel(render::framebuffer &target, vec2<int> const &minimum, vec2<int> const &maximum, uint8_t const light, uint8_t const dark) {
  // Native order also determines the colours at the shared corner pixels.
  draw_line(target, vec2<int>{minimum}, {maximum.x, minimum.y}, light);
  draw_line(target, {maximum.x, minimum.y}, vec2<int>{maximum}, dark);
  draw_line(target, vec2<int>{maximum}, {minimum.x, maximum.y}, dark);
  draw_line(target, {minimum.x, maximum.y}, vec2<int>{minimum}, light);
}
vec2<int> interior(dialogue_form const &form, dialogue_widget const &widget) {
  return {(form.position.x + widget.position.x + 2) * dialogue_cell_pixels, (form.position.y + widget.position.y + 2) * dialogue_cell_pixels};
}

} // anonymous namespace

void draw_dialogue_form(render::framebuffer &target, dialogue_form const &form) {
  vec2<int> const minimum{form.position.x * dialogue_cell_pixels + 7, form.position.y * dialogue_cell_pixels + 7};
  vec2<int> const maximum{(form.position.x + form.size.x + 2) * dialogue_cell_pixels - 8, (form.position.y + form.size.y + 2) * dialogue_cell_pixels - 8};
  fill(target, minimum + vec2<int>{1, 1}, maximum - vec2<int>{1, 1}, static_cast<uint8_t>(dialogue_palette_base + form.background));
  bevel(target, minimum, maximum, dialogue_palette_base + 13, dialogue_palette_base + 5);
  if(form.border == dialogue_border::double_bevel) bevel(target, minimum - vec2<int>{1, 1}, maximum + vec2<int>{1, 1}, dialogue_palette_base + 13, dialogue_palette_base + 5);
}
void draw_dialogue_widget(render::framebuffer &target, widget_font const &font, dialogue_form const &form, dialogue_widget const &widget) {
  auto const position{interior(form, widget)};
  vec2<int> const extent{widget.size.x * dialogue_cell_pixels, widget.size.y * dialogue_cell_pixels};
  fill(target, position, position + extent - vec2<int>{1, 1}, static_cast<uint8_t>(dialogue_palette_base + widget.background));
  uint8_t const first{static_cast<uint8_t>(dialogue_palette_base + (widget.border == dialogue_border::raised ? 13 : widget.border == dialogue_border::sunken ? 5 : 10))};
  uint8_t const second{static_cast<uint8_t>(dialogue_palette_base + (widget.border == dialogue_border::raised ? 5 : widget.border == dialogue_border::sunken ? 13 : 10))};
  bevel(target, position - vec2<int>{1, 1}, position + extent, first, second);
  int const x{position.x + (widget.left_aligned ? 0 : widget.size.x * 4 - font.measure(widget.text) / 2)};
  int y{position.y + 1};
  std::string_view text{widget.text};
  while(!text.empty()) {
    auto const end{text.find_first_of("\r\n")};
    font.draw(target, {x, y}, text.substr(0, end), static_cast<uint8_t>(dialogue_palette_base + widget.foreground));
    if(end == std::string_view::npos) break;
    unsigned int const skip{text[end] == '\r' ? 2u : 1u};
    text.remove_prefix(std::min(text.size(), end + skip));
    y += dialogue_cell_pixels;
  }
}
bool dialogue_contains(dialogue_form const &form, dialogue_widget const &widget, vec2<int16_t> const &point) {
  auto const position{interior(form, widget)};
  return point.x >= position.x - 1 && point.y >= position.y - 1 && point.x <= position.x + widget.size.x * dialogue_cell_pixels && point.y <= position.y + widget.size.y * dialogue_cell_pixels;
}

} // namespace tfx::presentation
