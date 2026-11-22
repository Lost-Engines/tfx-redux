#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/player.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
tfx::resources::bytes font_image() {
  tfx::resources::bytes image(973476 + 0xe100);
  image[0x70aa4 + 0x5bb51 + 1] = 1;                                            // A single synthetic fixed-font ink pixel.
  return image;
}
} // anonymous namespace

int main() try {
  using tfx::presentation::clip;
  using tfx::presentation::dac_palette;
  using tfx::presentation::fonts;
  using tfx::presentation::node_kind;
  using tfx::presentation::playback_phase;
  using tfx::presentation::scene_input;
  using tfx::presentation::scene_key;
  using tfx::presentation::scene_player;
  using tfx::presentation::sequence_controls;
  using tfx::presentation::text_alignment;
  using tfx::presentation::text_block;
  using tfx::presentation::variables;
  tfx::resources::bytes screen(772 + 64000, 0);
  screen[0] = 'w';
  screen[1] = 'a';
  screen[2] = 'r';
  screen[3] = ' ';
  for(unsigned int i{4}; i != 772; ++i) screen[i] = 63;
  std::fill(screen.begin() + 772, screen.end(), uint8_t{2});
  fonts const fonts{font_image()};
  variables state;
  dac_palette palette;
  palette.fill(63);
  scene_input input;
  sequence_controls sequence;
  clip image{
    .fade_in{1},
    .fade_out{1},
    .wait_ticks{2},
    .text{text_block{
      .position{0, 0},
      .alignment{text_alignment::left},
      .shadow{false},
      .reveal{true},
      .font{1},
      .colour{9},
      .text{"AAAAAAAAAA"},
    }},
  };
  scene_player still{node_kind::image, image, screen, fonts, state, {}, palette, palette, {0, 0}, input};
  for(unsigned int tick{1}; tick != 41; ++tick) {
    require(!still.advance({tick, tick}, input, sequence), "fade-in ended the scene");
    require(still.palette()[0] == 63 * tick / 40, "fade must round in the six-bit DAC domain");
    require(still.visible_characters() == 0, "image reveal must start after fade-in");
  }
  require(still.stage() == playback_phase::content, "fade did not reach content");
  still.advance({41, 40}, input, sequence);
  require(still.visible_characters() == 0 && still.stage() == playback_phase::content, "still countdown advanced during reveal");
  input.key = scene_key::space;
  still.advance({42, 41}, input, sequence);
  require(still.visible_characters() == 10 && input.key == scene_key::none && still.stage() == playback_phase::content, "first Space must reveal and consume its key");
  require(still.image().pixels[0] == 9 && still.image().pixels[1] == 2, "caption must compose over retained indexed pixels");
  still.advance({43, 42}, input, sequence);
  require(still.stage() == playback_phase::fade_out, "still wait did not resume after full reveal");
  for(unsigned int tick{0}; tick != 40; ++tick) still.advance({44 + tick, 43 + tick}, input, sequence);
  require(still.stage() == playback_phase::complete && still.palette()[0] == 0 && still.image().pixels[0] == 0, "fade-out must clear the DAC and drawing page");
  clip text{
    .wait_ticks{0},
    .text{image.text},
  };
  scene_player text_only{node_kind::text, text, {}, fonts, state, {}, palette, palette, {0, 0}, input};
  for(unsigned int tick{1}; tick != 41; ++tick) text_only.advance({tick, tick}, input, sequence);
  text_only.advance({41, 41}, input, sequence);
  require(text_only.visible_characters() == 10 && text_only.stage() == playback_phase::fade_out, "text-only reveal must retain elapsed fade time and execute its zero-wait iteration");
  clip instant{.wait_ticks{5}};
  scene_player skip{node_kind::image, instant, screen, fonts, state, {}, palette, palette, {0, 0}, input};
  input.mouse_buttons = 3;
  require(skip.advance({0, 0}, input, sequence) && !sequence.skipped, "left mouse must win over simultaneous right mouse");
  std::cout << "Scene phases, native palette rounding, reveal/hold/Space ordering and terminal clears passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
