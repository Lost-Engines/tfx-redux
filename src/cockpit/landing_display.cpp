#include "landing_display.h"
#include <utility>
#include "cockpit/format.h"
#include "presentation/drawing.h"
#include "render/sprites.h"

namespace tfx::cockpit {

void draw_landing_cue(render::software_renderer &renderer, instrument_assets const &assets,
    flight::landing_cue const &cue, vec2<int16_t> const &offset, render::framebuffer *const coverage) {
  auto const centred{[&](std::string const &message, int const y){
    int const x{160 - static_cast<int16_t>(renderer.text_width(message)) / 2 + offset.x};
    renderer.text(math::signed_word(static_cast<uint16_t>(x)), math::signed_word(static_cast<uint16_t>(y + offset.y)), message, 6, coverage);
  }};
  centred(detail::formatted(assets.landing_heading_formats[cue.beacon ? 0 : 1], cue.heading), 92);
  if(cue.beacon) {
    if(cue.projected.valid) {
      auto const &point{cue.projected.point};
      auto const line{[&](vec2<int> const &a, vec2<int> const &b){
        presentation::draw_clipped_line(renderer.output, vec2<int>{a}, vec2<int>{b}, {0, 0}, {319, 199}, 6);
        if(coverage) presentation::draw_clipped_line(*coverage, vec2<int>{a}, vec2<int>{b}, {0, 0}, {319, 199}, 255);
      }};
      line({point.x - 5, point.y - 5}, {point.x + 5, point.y + 5});
      line({point.x + 5, point.y - 5}, {point.x - 5, point.y + 5});
    }
  } else {
    if(cue.projected.valid) {
      render::draw_monochrome(renderer.output, assets.landing_masks[0], {static_cast<uint16_t>(136 + offset.x), static_cast<uint16_t>(49 + cue.correction.y + offset.y)}, 6, coverage, renderer.clipping);
      render::draw_monochrome(renderer.output, assets.landing_masks[1], {static_cast<uint16_t>(159 - cue.correction.x + offset.x), static_cast<uint16_t>(26 + offset.y)}, 6, coverage, renderer.clipping);
    }
    if(cue.notice) centred(assets.landing_height_labels[std::to_underlying(*cue.notice)], 82);
  }
}

} // namespace tfx::cockpit
