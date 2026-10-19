#include "menu_player.h"
#include <algorithm>

namespace tfx::presentation {
menu_player::menu_player(menu_program source, std::span<uint8_t const> const media, variables &state,
  std::array<pointer_style,2> const &pointers, bios_counter const counter) : source{std::move(source)}, state{state}, pointer{pointers.at(this->source.cursor_style)} {
  std::ranges::copy(resources::binary_view{media}.slice(4,native_colours.size()),native_colours.begin());
  if(this->source.animation.empty()) background = resources::decode_screen(media).image;
  else {
    movie.emplace(media);
    movie->advance();
    background = movie->image();
    // Native menu AR0 playback advances one block per display flip, with the initial frame retained for fades.
    clock.emplace(static_cast<uint16_t>(movie->game_frames),0,counter);
  }
  display = background;
}
void menu_player::compose(vec2<int16_t> const &position, bool const pointer_visible) {
  display = background;
  if(selected && source.flash_box) {
    auto const &choice{source.choices[*selected]};
    auto const rectangle{[&](int const offset, uint8_t const colour) {
      int const left{choice.minimum.x + offset}, right{choice.maximum.x + offset}, top{choice.minimum.y + offset}, bottom{choice.maximum.y + offset};
      auto const pixel{[&](int const x, int const y) {
        if(x >= 0 && x < render::screen_width && y >= 0 && y < render::screen_height) display.write(static_cast<unsigned int>(y * render::screen_width + x),colour);
      }};
      for(int x{std::max(0,left)}; x <= std::min(render::screen_width - 1,right); ++x) { pixel(x,top);pixel(x,bottom); }
      for(int y{std::max(0,top)}; y <= std::min(render::screen_height - 1,bottom); ++y) { pixel(left,y);pixel(right,y); }
    }};
    rectangle(1,0);
    uint8_t constexpr default_box_colour{15}; // No shipped script changes native boxcol.
    rectangle(0,default_box_colour);
  }
  if(pointer_visible) draw_pointer(display,selected || source.hit_test(position) ? pointer.active : pointer.idle,position);
}
bool menu_player::advance(bios_counter const counter, vec2<int16_t> const &position, bool const left_down, bool const pointer_visible) {
  if(stage == phase::complete) return true;
  if(stage == phase::entering) {
    displayed_colours = fade_palette(native_colours,++fade_level,40);
    if(fade_level == 40) stage = phase::choosing;
    // Animated menus draw their pointer before the ramp; static menus introduce it afterwards.
    compose(position,pointer_visible && movie.has_value());
    return false;
  }
  if(stage == phase::leaving) {
    displayed_colours = fade_palette(native_colours,fade_level--,40);
    if(!fade_level) { display.pixels.fill(0);displayed_colours.fill(0);stage = phase::complete; }
    return stage == phase::complete;
  }
  if(movie) {
    auto const step{clock->advance(counter)};
    if(step.changed) { movie->advance();if(step.traversal_complete) movie->repeat(); }
    background = movie->image();
  }
  if(stage == phase::choosing && left_down && pointer_visible) {
    if(auto const hit{source.hit_test(position)}) {
      selected = hit;
      source.choose(*hit,state);
      stage = phase::releasing;
    }
  } else if(stage == phase::releasing && !left_down) {
    // Native 0xd5750 applies the choice on press, then waits for release before fading away.
    stage = phase::leaving;
    fade_level = 40;
  }
  compose(position,pointer_visible);
  return false;
}
render::framebuffer const &menu_player::image() const { return display; }
render::palette menu_player::colours() const { return render::decode_palette(displayed_colours); }
std::optional<unsigned int> menu_player::choice() const { return selected; }
} // namespace tfx::presentation
