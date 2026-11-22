#include "hud_alerts.h"
#include <utility>
#include "presentation/drawing.h"

namespace tfx::cockpit {

hud_alerts::hud_alerts(instrument_assets const &source, hud_alert_state const initial)
  : assets{source},
    state{initial} {}
hud_alert_state const &hud_alerts::value() const {
  return state;
}
void hud_alerts::show(hud_alert const alert) {
  simulation::timer_ticks constexpr alert_duration{108};
  state.alert = alert;
  state.remaining = static_cast<int16_t>(alert_duration);
}
void hud_alerts::check_fuel(double const fuel) {
  // Native 0x800f4 requests BINGO before the failed/intermittent HUD gate.
  if(!(fuel >= assets.bingo_fuel)) show(hud_alert::bingo);
}
void hud_alerts::draw(render::software_renderer &renderer, vec2<int16_t> const &offset, simulation::timer_ticks const elapsed, render::framebuffer *const coverage) {
  if(!state.remaining) return;
  auto const point{[&](vec2<int16_t> const &vertex){
    return vec2<int>{math::signed_word(static_cast<uint16_t>(vertex.x + offset.x)), math::signed_word(static_cast<uint16_t>(vertex.y + offset.y))};
  }};
  uint8_t constexpr colour{6};
  for(auto const &line : assets.hud_alert_shapes[std::to_underlying(state.alert)]) {
    presentation::draw_line(renderer.output, point(line.from), point(line.to), colour);
    if(coverage) presentation::draw_line(*coverage, point(line.from), point(line.to), 255);
  }
  // Native SUB AX wraps before the signed expiry check, even for long frames.
  state.remaining = math::signed_word(static_cast<uint16_t>(state.remaining - elapsed));
  if(state.remaining < 0) state.remaining = 0;
}

} // namespace tfx::cockpit
