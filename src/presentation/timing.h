#pragma once

#include "presentation/program.h"
#include "resources/media.h"

namespace tfx::presentation {

enum class scene_key : uint8_t {
  none,
  escape = 1,
  quit = 0x10,
  space = 0x39
};
struct scene_input {
  scene_key key{scene_key::none};
  bool shift{false};
  uint8_t mouse_buttons{0};
};
struct sequence_controls {
  bool skipped{false};
  bool quit{false};
};
bool advance_requested(scene_input &input, sequence_controls &sequence, bool revealing, uint16_t visible, uint16_t characters);

using bios_counter = uint32_t;
struct animation_step {
  bool changed{false};
  bool traversal_complete{false};
  unsigned int block{0};
};
class animation_clock {
private:
  uint16_t frames;
  uint16_t delay;
  uint16_t ordinal{2};
  bios_counter previous;

public:
  animation_clock(uint16_t frames, uint16_t delay, bios_counter initial_counter);
  animation_step advance(bios_counter counter);
  bios_counter last_counter() const;
  uint16_t next_ordinal() const;
};
class scene_wait {
private:
  node_kind kind;
  int32_t ticks;

public:
  scene_wait(node_kind kind, int16_t ticks);
  bool advance(bool revealing);
  int32_t remaining() const;
};
using dac_palette = render::dac_palette;
dac_palette fade_palette(dac_palette const &source, unsigned int level, unsigned int stages);

} // namespace tfx::presentation
