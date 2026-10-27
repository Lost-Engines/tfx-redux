#include "arming_screen.h"
#include <algorithm>
#include <utility>
#include "render/view_settings.h"
#include "presentation/cursor.h"

namespace tfx::presentation {

arming_screen::arming_screen(resources::archive const &game, resources::executable_tables const &tables, weapons::armament &inventory,
  flight::player_aircraft const aircraft, unsigned int const default_fit, arming_entry const entry, vec2<int> const &resolution) :
  inventory{inventory}, tables{tables}, profile{tables.airframes.at(std::to_underlying(aircraft) - 1)},
  controls{inventory,tables.arming_art(),default_fit,entry}, preview{game,tables,aircraft}, renderer{tables}, detail_renderer{tables}, resolution{resolution} {
  detail_renderer.output.resize(resolution);
  detail_renderer.clipping = render::raster_clip::viewport({0,0},{resolution.x - 1,resolution.y - 1});
  auto const bytes{game.read("iff/raw/" + tables.arming_art().backdrop)};
  background = resources::decode_screen(bytes);
  std::ranges::copy(resources::binary_view{bytes}.slice(4,palette.size()),palette.begin());
  // Native 8B4D7 runs the entry callback once before preparing both display pages.
  controls.advance({100,75},{0,0},0);
  // The original prepares both display pages before the forty-retrace fade-in.
  draw({100,75},{0,0},0);
  draw({100,75},{0,0},0);
}
void arming_screen::draw(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t const buttons) {
  // Native 8B591 handles right-click cancellation before drawing the callback.
  bool const dismiss_notice{controls.state().mode == arming_mode::mission_incomplete && (buttons & 2)};
  if(dismiss_notice) controls.cancel();
  std::optional<unsigned int> picked;
  auto const mode{controls.state().mode};
  if(mode == arming_mode::placing || mode == arming_mode::clearing) {
    picked = preview.pick_cursor(position,controls.state());
  }
  renderer.output = background.image;
  bool const expanded{resolution != vec2<int>{render::screen_width,render::screen_height}};
  if(expanded) {
    detail_renderer.output = render::scale_image(renderer.output,resolution);
    preview.draw_models(detail_renderer,controls.state(),inventory,controls.previews());
    renderer.output.track_writes();
  } else preview.draw_models(renderer,controls.state(),inventory,controls.previews());
  previous_drag = draw_arming_widgets(renderer,tables.arming_art(),controls.state(),inventory,tables.weapons,profile,previous_drag).drag;
  if(mode == arming_mode::mission_incomplete) draw_arming_mission_incomplete(renderer,tables.arming_art());
  if(mode == arming_mode::unavailable) draw_arming_unavailable(renderer,tables.arming_art());
  if(mode == arming_mode::overview) for(unsigned int slot{0}; slot != flight::store_slots; ++slot) {
    auto const &store{inventory.pylons[slot]};
    if(store.weapon != weapons::empty_pylon && store.count) draw_arming_store_label(renderer,tables.weapons[store.weapon].label,vec2<int>{tables.arming_art().slot_labels[slot]});
  }
  if(mode == arming_mode::clearing && picked) {
    auto const &store{inventory.pylons[*picked]};
    if(store.weapon != weapons::empty_pylon && store.count) draw_arming_store_label(renderer,tables.weapons[store.weapon].label,{30,160});
  }
  controls.advance(position,movement,dismiss_notice ? static_cast<uint8_t>(buttons & ~2) : buttons,picked);
  if(mode == arming_mode::confirm_return) {
    for(int y{83}; y <= 128; ++y) for(int x{32}; x <= 287; ++x) renderer.output.write(static_cast<size_t>(y * render::screen_width + x),76);
    renderer.text(44,91,"RETURN TO MISSION WITHOUT REARMING?",15);
    renderer.text(103,113,"YES",15);renderer.text(180,113,"NO",15);
    draw_pointer(renderer.output,tables.pointers.front().idle,position);
  }
  if(expanded) render::composite_interface(detail_renderer.output,renderer.output);
  // The 3D cursor belongs above both the preview and its overlaid UI labels.
  if(mode != arming_mode::mission_incomplete && mode != arming_mode::confirm_return) preview.draw_cursor(expanded ? detail_renderer : renderer,controls.state(),position);
  if(expanded) composed = detail_renderer.output;
}
void arming_screen::press(uint8_t const scan) { key = scan; }
bool arming_screen::rotating() const {
  return controls.state().mode == arming_mode::rotating;
}
bool arming_screen::advance(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t const buttons) {
  if(entering) {
    if(fade_level < 40) ++fade_level;
    else { entering = false;preview.start(); }
    return false;
  }
  if(leaving) {
    if(fade_level) --fade_level;
    return fade_level == 0;
  }
  controls.press(std::exchange(key,0));
  preview.advance();
  draw(position,movement,buttons);
  if(controls.state().mode == arming_mode::complete || cancelled()) leaving = true;
  return false;
}
bool arming_screen::cancelled() const { return controls.state().mode == arming_mode::cancelled; }
void arming_screen::resize(vec2<int> const &size) {
  if(resolution == size) return;
  resolution = vec2<int>{size};
  detail_renderer.output.resize(size);
  detail_renderer.clipping = render::raster_clip::viewport({0,0},{size.x - 1,size.y - 1});
}
render::framebuffer const &arming_screen::image() const { return resolution == vec2<int>{render::screen_width,render::screen_height} ? renderer.output : composed; }
render::palette arming_screen::colours() const { return render::decode_palette(fade_palette(palette,fade_level,40)); }

} // namespace tfx::presentation
