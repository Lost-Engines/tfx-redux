#include "hud_text.h"
#include "cockpit/format.h"
#include "presentation/drawing.h"
#include "render/markers.h"
#include "render/sprites.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace tfx::cockpit {
hud_text::hud_text(instrument_assets const &source) : assets{source} {
}
void hud_text::draw_aerodynamic(render::software_renderer &renderer, aerodynamic_hud_state const &state, vec2<int16_t> const &offset, render::framebuffer *const coverage) const {
  // Native 0x7f870/0x7e130/0x7e328: angle of attack at left, climb in ft/s at right.
  // These are the original double literals, including its approximate pi.
  double constexpr degrees_per_half_turn{180}, native_pi{3.14159}, zero_alpha_row{60};
  double constexpr feet_per_metre{3.281};
  auto const word{[](int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }};
  auto const truncate{[](double const value) {
    if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(value);
  }};
  auto const mask{[&](instrument_mask const shape, vec2<int> const &position) {
    render::draw_monochrome(renderer.output,assets.masks[std::to_underlying(shape)],{word(position.x + offset.x),word(position.y + offset.y)},6,coverage,renderer.clipping);
  }};
  auto const line{[&](vec2<int> const &from, vec2<int> const &to) {
    vec2<int> const a{word(from.x + offset.x),word(from.y + offset.y)}, b{word(to.x + offset.x),word(to.y + offset.y)};
    vec2<int> const low{0,2 + offset.y}, high{319,144 + offset.y};
    presentation::draw_clipped_line(renderer.output,vec2<int>{a},vec2<int>{b},low,high,6);
    if(coverage) presentation::draw_clipped_line(*coverage,vec2<int>{a},vec2<int>{b},low,high,255);
  }};
  auto const point{[&](vec2<int> const &position) {
    render::draw_marker(renderer.output,{static_cast<uint16_t>(position.x + offset.x),static_cast<uint16_t>(position.y + offset.y)},6,render::marker_kind::pixel,coverage);
  }};
  mask(instrument_mask::angle_of_attack_scale,{83,10});
  // Native divides stored doubles before the extended multiply and truncation.
  int const alpha{word(truncate(zero_alpha_row - (degrees_per_half_turn / static_cast<double>(native_pi)) * state.angle_of_attack))};
  line({93,60},{93,alpha});point({92,alpha});point({94,alpha});
  mask(instrument_mask::climb_rate_scale,{222,2});
  int const climb{word(truncate(-static_cast<double>(state.down_metres_per_second) * feet_per_metre))};
  int const climb_row{word(26 - climb)};
  // C4CB6 bypasses viewport clipping here. Its linear VGA writes can wrap
  // to the following scanline at the right edge, as the endpoint pixels do.
  int const end{std::clamp(climb_row,6,46)};
  for(int row{std::min(26,end)}; row <= std::max(26,end); ++row) point({237,row});
  if(climb_row >= 6 && climb_row <= 46) { point({236,climb_row});point({238,climb_row}); }
  auto const label{detail::formatted(assets.autopilot_number_format,climb)};
  renderer.text(word(220 - static_cast<int>(renderer.text_width(label)) + offset.x),word(21 + offset.y),label,6,coverage);
  mask(instrument_mask::climb_rate_label,{202,19});
}
void hud_text::draw_values(render::software_renderer &renderer, hud_text_state const &state, render::framebuffer *const coverage) const {
  uint8_t constexpr colour{6};
  auto const text{[&](vec2<int> const &local, std::string const &message) {
    int const x{math::signed_word(static_cast<uint16_t>(local.x + state.offset.x))};
    int const y{math::signed_word(static_cast<uint16_t>(local.y + state.offset.y))};
    renderer.text(x,y,message,colour,coverage);
  }};
  auto const right{[&](vec2<int> const &edge, std::string const &message) {
    text({math::signed_word(static_cast<uint16_t>(edge.x - renderer.text_width(message))),edge.y},message);
  }};
  // Native 0x7f8e0: every formatted product crosses its original double store.
  text({98,54},detail::decimal(assets.hud_number_formats[0],static_cast<double>(-static_cast<double>(state.height) * assets.hud_altitude_scale),0));
  text({204,54},detail::decimal(assets.hud_number_formats[0],static_cast<double>(static_cast<double>(state.speed) * assets.speed_scale),0));
  text({204,63},detail::decimal(assets.hud_number_formats[1],state.mach,2));
  right({230,80},detail::decimal(assets.hud_number_formats[2],static_cast<double>(state.load),1));
  int constexpr world_units_per_cell{2048};
  right({217,86},detail::formatted(assets.hud_number_formats[3],static_cast<long>(state.position.x / world_units_per_cell)));
  right({230,86},detail::formatted(assets.hud_number_formats[3],static_cast<long>(state.position.z / world_units_per_cell)));
  text({96,79},detail::decimal(assets.hud_number_formats[4],static_cast<double>(static_cast<double>(state.throttle) * assets.hud_throttle_scale),0));
}
void hud_text::draw_messages(render::software_renderer &renderer, hud_text_state const &state, render::framebuffer *const coverage) const {
  uint8_t constexpr colour{6};
  auto const text{[&](vec2<int> const &local, std::string const &message) {
    int const x{math::signed_word(static_cast<uint16_t>(local.x + state.offset.x))};
    int const y{math::signed_word(static_cast<uint16_t>(local.y + state.offset.y))};
    renderer.text(x,y,message,colour,coverage);
  }};
  // Native 0x7eb7c: the three messages have independent eligibility and rows.
  std::array<bool,3> const enabled{state.air_brake,state.brake_chute,state.arrester_cable};
  int constexpr message_first_row{29}, message_spacing{7};
  for(unsigned int index{0}; index != enabled.size(); ++index) if(enabled[index]) {
    auto const &message{assets.brake_messages[index]};
    int const width{math::signed_word(static_cast<uint16_t>(renderer.text_width(message)))};
    text({render::centre_x - width / 2,message_first_row + static_cast<int>(index) * message_spacing},message);
  }
}

void hud_text::draw(render::software_renderer &renderer, hud_text_state const &state, render::framebuffer *const coverage) const {
  draw_values(renderer,state,coverage);
  draw_messages(renderer,state,coverage);
}

} // namespace tfx::cockpit
