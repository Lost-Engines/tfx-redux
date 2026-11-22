#include "player.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tfx::presentation {
namespace {
unsigned int stages(uint8_t const mode) {
  return mode == 1 ? 40u : mode == 2 ? 80u : 0u;
}
dac_palette media_palette(std::span<uint8_t const> const media) {
  resources::binary_view const view{media};
  dac_palette result;
  std::ranges::copy(view.slice(4,result.size()),result.begin());
  // Validate even when a scene has no ramp.
  return fade_palette(result,1,1);
}
} // anonymous namespace

scene_player::scene_player(node_kind const type, clip const &clip, std::span<uint8_t const> const media,
  fonts const &fonts, variables const &state, external_text resolve,
  dac_palette const &current_palette, dac_palette const &text_palette,
  scene_counters const counters, scene_input &input) : kind{type}, source{clip}, font_data{fonts},
  caption{}, wait{kind,source.wait_ticks}, native_colours{kind == node_kind::text ? (source.default_text_palette ? text_palette : current_palette) : media_palette(media)},
  outgoing_colours{kind == node_kind::text ? current_palette : native_colours}, displayed_colours{},
  phase{playback_phase::fade_in}, fade_stages{kind == node_kind::text ? 40 : stages(source.fade_in)} {
  if(source.text) caption = prepare_text(*source.text,state,font_data,std::move(resolve));
  if(kind == node_kind::image) background = resources::decode_screen(media).image;
  else if(kind == node_kind::animation) {
    movie.emplace(media);
    if(!movie->advance()) throw std::runtime_error{"presentation animation has no first frame"};
    background = movie->image();
    clock.emplace(static_cast<uint16_t>(movie->game_frames),source.frame_delay,counters.bios);
  }
  display = background;
  // Text-only scenes start their caption before the fade; image/movie reveals start after it.
  if(source.text && (!source.text->reveal || kind == node_kind::text)) {
    if(source.text->reveal) reveal.emplace(caption.characters,counters.text);
    draw_caption(counters,input);
  }
  if(!fade_stages) {
    displayed_colours = native_colours;
    begin_content(counters);
  }
}

void scene_player::begin_content(scene_counters const counters) {
  phase = playback_phase::content;
  if(source.text && source.text->reveal && !reveal) reveal.emplace(caption.characters,counters.text);
}
void scene_player::draw_caption(scene_counters const counters, scene_input &input) {
  if(!source.text) return;
  if(reveal && reveal->advance(counters.text,input.key == scene_key::space)) input.key = scene_key::none;
  draw_text(display,caption,font_data,visible_characters());
}
void scene_player::finish() {
  fade_stages = kind == node_kind::text ? 40 : stages(source.fade_out);
  fade_level = fade_stages;
  phase = fade_stages ? playback_phase::fade_out : playback_phase::complete;
}

bool scene_player::advance(scene_counters const counters, scene_input &input, sequence_controls &sequence) {
  if(phase == playback_phase::complete) return true;
  if(phase == playback_phase::fade_in) {
    displayed_colours = fade_palette(native_colours,++fade_level,fade_stages);
    if(fade_level == fade_stages) begin_content(counters);
    return false;
  }
  if(phase == playback_phase::fade_out) {
    displayed_colours = fade_palette(outgoing_colours,fade_level--,fade_stages);
    if(!fade_level) {
      // Native fade-out clears the drawing page and DAC immediately after the last upload.
      display.pixels.fill(0);
      displayed_colours.fill(0);
      phase = playback_phase::complete;
    }
    return phase == playback_phase::complete;
  }
  bool traversal_complete{false};
  if(movie) {
    auto const step{clock->advance(counters.bios)};
    if(step.changed) {
      if(movie->position() != step.block || !movie->advance()) throw std::runtime_error{"presentation animation clock left its declared blocks"};
      if(step.traversal_complete) movie->repeat();
    }
    traversal_complete = step.traversal_complete;
    background = movie->image();
  }
  display = background;
  // Static captions are retained on stills; movies rebuild and redraw them every flip.
  if(source.text && (source.text->reveal || kind != node_kind::image)) draw_caption(counters,input);
  else if(source.text) draw_text(display,caption,font_data);
  bool const revealing{source.text && source.text->reveal && math::signed_word(visible_characters()) < math::signed_word(caption.characters)};
  bool const expired{movie && source.play_until_end ? traversal_complete : wait.advance(revealing)};
  if(expired || advance_requested(input,sequence,source.text && source.text->reveal,visible_characters(),caption.characters)) finish();
  return phase == playback_phase::complete;
}

render::framebuffer const &scene_player::image() const {
  return display;
}
dac_palette const &scene_player::palette() const {
  return displayed_colours;
}
render::palette scene_player::colours() const {
  return render::decode_palette(displayed_colours);
}
playback_phase scene_player::stage() const {
  return phase;
}
uint16_t scene_player::visible_characters() const {
  return reveal ? reveal->state().visible : source.text && source.text->reveal ? 0 : caption.characters;
}

} // namespace tfx::presentation
