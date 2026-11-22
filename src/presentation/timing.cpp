#include "timing.h"
#include <bit>
#include <stdexcept>

namespace tfx::presentation {

bool advance_requested(scene_input &input, sequence_controls &sequence, bool const revealing, uint16_t const visible, uint16_t const characters) {
  // Native 0xb1128 keeps the last scan code separate from held shift/mouse state.
  if(input.shift && input.key == scene_key::quit) {
    input.key = scene_key::none;
    sequence.quit = true;
    return true;
  }
  if(input.key == scene_key::space && revealing && math::signed_word(visible) < math::signed_word(characters)) return false;
  if(input.key == scene_key::space) { input.key = scene_key::none;return true; }
  if(input.key == scene_key::escape) { sequence.skipped = true;return true; }
  if(input.mouse_buttons & 1) return true;
  if(input.mouse_buttons & 2) { sequence.skipped = true;return true; }
  return false;
}

animation_clock::animation_clock(uint16_t const frames, uint16_t const delay, bios_counter const initial_counter) : frames{frames}, delay{delay}, previous{initial_counter} {
  if(frames < 2) throw std::runtime_error{"native animation playback requires at least two declared frames"};
}

animation_step animation_clock::advance(bios_counter const counter) {
  // Native 0xc9fac advances at most one stored block, discards timing backlog, and repeats from frame 2.
  if(std::bit_cast<int32_t>(counter - previous) < delay) return {};
  previous = counter;
  animation_step const result{
    .changed{true},
    .traversal_complete{ordinal == frames},
    .block{static_cast<unsigned int>(ordinal - 1)},
  };
  if(result.traversal_complete) ordinal = 2;
  else ++ordinal;
  return result;
}

bios_counter animation_clock::last_counter() const {
  return previous;
}
uint16_t animation_clock::next_ordinal() const {
  return ordinal;
}

scene_wait::scene_wait(node_kind const kind, int16_t const ticks) : kind{kind}, ticks{ticks} {
  if(kind != node_kind::text && kind != node_kind::image && kind != node_kind::animation) throw std::runtime_error{"scene wait requires a visual parser node"};
}

bool scene_wait::advance(bool const revealing) {
  // Native text tests its previous dword after decrement; images/movies test the new signed count.
  uint32_t const before{std::bit_cast<uint32_t>(ticks)};
  if(kind != node_kind::image || !revealing) ticks = std::bit_cast<int32_t>(before - 1);
  return kind == node_kind::text ? before == 0 : ticks <= 0;
}

int32_t scene_wait::remaining() const {
  return ticks;
}

dac_palette fade_palette(dac_palette const &source, unsigned int const level, unsigned int const stages) {
  // Native 0xca2f4/35c/3c0/428 scale the six-bit DAC entries before any display RGB expansion.
  unsigned int constexpr maximum_fade_stages{80}, maximum_dac_value{63};
  if(!stages || stages > maximum_fade_stages || level > stages) throw std::runtime_error{"invalid native palette fade stage"};
  dac_palette result;
  for(unsigned int index{0}; index != source.size(); ++index) {
    if(source[index] > maximum_dac_value) throw std::runtime_error{"palette fade source exceeds the VGA DAC range"};
    result[index] = static_cast<uint8_t>(source[index] * level / stages);
  }
  return result;
}

} // namespace tfx::presentation
