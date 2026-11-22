#pragma once

#include "presentation/text.h"
#include "presentation/timing.h"
#include "resources/media.h"

namespace tfx::presentation {

struct scene_counters {
  bios_counter bios;
  text_counter text;
};
enum class playback_phase : uint8_t {
  fade_in,
  content,
  fade_out,
  complete
};

class scene_player {
private:
  node_kind kind;
  clip source;
  fonts const &font_data;
  prepared_text caption;
  std::optional<text_reveal> reveal;
  std::optional<resources::animation> movie;
  std::optional<animation_clock> clock;
  scene_wait wait;
  render::framebuffer background;
  render::framebuffer display;
  dac_palette native_colours;
  dac_palette outgoing_colours;
  dac_palette displayed_colours;
  playback_phase phase;
  unsigned int fade_stages;
  unsigned int fade_level{0};
  void begin_content(scene_counters counters);
  void draw_caption(scene_counters counters, scene_input &input);
  void finish();

public:
  scene_player(node_kind kind, clip const &source, std::span<uint8_t const> media,
    fonts const &fonts, variables const &state, external_text resolve,
    dac_palette const &current_palette, dac_palette const &text_palette,
    scene_counters counters, scene_input &input);
  // One native content iteration or palette-ramp retrace; clocks are supplied by the host.
  bool advance(scene_counters counters, scene_input &input, sequence_controls &sequence);
  render::framebuffer const &image() const;
  dac_palette const &palette() const;
  render::palette colours() const;
  playback_phase stage() const;
  uint16_t visible_characters() const;
};

} // namespace tfx::presentation
