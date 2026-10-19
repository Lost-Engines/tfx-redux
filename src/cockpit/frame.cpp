#include "frame.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::cockpit {

frame::frame(instrument_assets const &source, resources::archive const &game) : assets{source} {
  for(unsigned int index{0}; index != views.size(); ++index) {
    auto const &layout{assets.viewports[index]};
    // The missile view has no footer; its .hdd is a five-byte placeholder,
    // uploaded but never read by the original viewport copy.
    if(!layout.panel.empty() && layout.bottom < render::screen_height - 1) {
      auto const raw{game.read(layout.panel)};
      if(raw.size() % render::screen_width) throw std::runtime_error{"cockpit panel has an incomplete planar row"};
      auto const height{static_cast<unsigned int>(raw.size() / render::screen_width)};
      if(height > render::screen_height) throw std::runtime_error{"cockpit panel exceeds its screen"};
      views[index].panel.emplace(resources::indexed_image{
        .size{render::screen_width,height},
        .pixels{resources::deplane(raw,{render::screen_width,height})},
      });
    }
    if(!layout.overlay.empty()) views[index].overlay.emplace(game.read(layout.overlay));
  }
}
vec2<int16_t> frame::clamp(unsigned int const view, vec2<int16_t> const &offset) const {
  auto const &layout{assets.viewports.at(view)};
  // Native 0x74104 first clamps to zero, then to row 198 minus the viewport
  // bottom. Full-height views consequently retain its original -1 result.
  int y{std::max(0,static_cast<int>(offset.y))};
  if(y + layout.bottom > render::screen_height - 2) y = render::screen_height - 2 - layout.bottom;
  return {offset.x,static_cast<int16_t>(y)};
}
vec2d frame::projection_origin(unsigned int const view, vec2<int16_t> const &offset) const {
  auto const &layout{assets.viewports.at(view)};
  return {render::centre_x - 1,static_cast<double>(layout.projection_centre_y + (layout.panel.empty() ? 0 : offset.y / 2))};
}
void frame::paint(render::framebuffer &target, unsigned int const view, vec2<int16_t> const &offset, render::framebuffer *const coverage, panel_copy const copy) const {
  auto const &layout{assets.viewports.at(view)};
  auto const &art{views.at(view)};
  if(art.overlay) {
    auto const image{art.overlay->image(offset.x / 4,offset.y)};
    image.paint(target);
    if(coverage) for(unsigned int index{0}; index != image.coverage.pixels.size(); ++index) if(image.coverage.pixels[index]) coverage->pixels[index] = 255;
  }
  // Native 0x74498/0x74104 copies the planar companion below the 3D viewport.
  // The live host retains all three pages and supplies the shared refresh
  // decision; isolated artwork callers request a copy by default.
  // Index zero is opaque panel colour.
  if(copy == panel_copy::preserve || !art.panel || layout.bottom >= render::screen_height - 1) return;
  auto const &panel{*art.panel};
  int const height{render::screen_height - 1 - layout.bottom};
  if(height > static_cast<int>(panel.size.y)) throw std::runtime_error{"cockpit panel does not cover its viewport footer"};
  int const top{layout.bottom + 1 + offset.y};
  for(int y{std::max(0,top)}; y < std::min(render::screen_height,top + height); ++y) {
    for(int x{std::max(0,static_cast<int>(offset.x))}; x < std::min(render::screen_width,render::screen_width + offset.x); ++x) {
      auto const source{static_cast<unsigned int>((y - top) * render::screen_width + x - offset.x)};
      auto const destination{static_cast<unsigned int>(y * render::screen_width + x)};
      target.write(destination,panel.pixels.at(source));
      if(coverage) coverage->pixels[destination] = 255;
    }
  }
}

cockpit_position::cockpit_position(instrument_assets const &source, vec2<int16_t> const &initial) : assets{source}, offset{initial} {
}
vec2<int16_t> const &cockpit_position::value() const { return offset; }
unsigned int cockpit_position::step_index() const { return step; }
cockpit_motion cockpit_position::movement() const { return motion; }
void cockpit_position::move_to(vec2<int16_t> const &position) { offset = vec2<int16_t>{position}; }
void cockpit_position::raise() { motion = cockpit_motion::up; }
void cockpit_position::lower() { motion = cockpit_motion::down; }
void cockpit_position::shake() {
  // Native 0x7d7e0 restores an interrupted shake before restarting its sequence.
  if(shake_origin.y != -1) offset = vec2<int16_t>{shake_origin};
  else shake_origin = vec2<int16_t>{offset};
  shake_step = 0;
  motion = cockpit_motion::flak_shake;
}
void cockpit_position::bump() { motion = cockpit_motion::ground_bump; }
void cockpit_position::advance(int16_t const viewport_bottom, bool const forward_cockpit) {
  int16_t constexpr end_of_shake{26};
  if(motion == cockpit_motion::ground_bump) {
    // Native 0x7d9d4 shares the flak cursor but tests DATA 0x6050 while
    // subtracting DATA 0x6044. Normally this gives +4, -4, then termination.
    // The following stop words overlap the live cursor and saved offsets.
    std::array<int16_t,6> const stop{
      assets.cockpit_bump_stop[0],assets.cockpit_bump_stop[1],assets.cockpit_bump_stop[2],
      static_cast<int16_t>(shake_step),shake_origin.x,shake_origin.y,
    };
    if(stop.at(shake_step) == end_of_shake) { motion = cockpit_motion::idle;shake_step = 0; }
    else offset.y = math::signed_word(static_cast<uint16_t>(offset.y - assets.cockpit_shake_steps.at(shake_step++)));
    return;
  }
  if(motion == cockpit_motion::flak_shake) {
    if(!forward_cockpit || assets.cockpit_shake_steps.at(shake_step) == end_of_shake) {
      motion = cockpit_motion::idle;
      shake_step = 0;
      // The original only restores X if Y changed; -1 is its saved-Y sentinel.
      if(offset.y != shake_origin.y) offset = vec2<int16_t>{shake_origin};
      shake_origin.y = -1;
    } else {
      offset.y = math::signed_word(static_cast<uint16_t>(offset.y - assets.cockpit_shake_steps.at(shake_step++)));
    }
    return;
  }
  if(motion == cockpit_motion::idle || viewport_bottom >= render::screen_height - 1) return;
  offset.y = assets.cockpit_steps.at(step);
  shake_origin = vec2<int16_t>{offset};
  if(motion == cockpit_motion::up) {
    if(offset.y > assets.cockpit_steps.front()) --step;
    else motion = cockpit_motion::idle;
  } else {
    if(offset.y < assets.cockpit_steps.back()) ++step;
    else motion = cockpit_motion::idle;
  }
}

} // namespace tfx::cockpit
