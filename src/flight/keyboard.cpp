#include "keyboard.h"
#include <utility>

namespace tfx::flight {
namespace {

enum class pan_bit : uint8_t { left = 1, right = 2, up = 4, down = 8, closer = 16, farther = 32, roll_left = 64, roll_right = 128 };
auto bit(bool const held, pan_bit const meaning)->uint8_t {
  return held ? std::to_underlying(meaning) : 0;
}

} // anonymous namespace

auto poll_keyboard(keyboard_state &state, keyboard_input const &input)->void {
  auto const directions{static_cast<uint8_t>(bit(input.directions.left,pan_bit::left) | bit(input.directions.right,pan_bit::right) | bit(input.directions.up,pan_bit::up) | bit(input.directions.down,pan_bit::down))};
  state.view_pan = input.right_shift ? static_cast<uint8_t>(directions | bit(input.home,pan_bit::closer) | bit(input.end,pan_bit::farther) | bit(input.page_up,pan_bit::roll_left) | bit(input.page_down,pan_bit::roll_right)) : uint8_t{0};
  // Native control panning preserves the incoming high two bits even when
  // Ctrl is released; the ordinary producer owns only these six low bits.
  uint8_t constexpr retained_target_bits{0xc0};
  state.target_pan &= retained_target_bits;
  if(input.control) state.target_pan |= static_cast<uint8_t>(bit(input.directions.left,pan_bit::left) | bit(input.directions.right,pan_bit::right) | bit(input.page_down,pan_bit::up) | bit(input.page_up,pan_bit::down) | bit(input.directions.up,pan_bit::closer) | bit(input.directions.down,pan_bit::farther));
  if(input.automatic_landing) return;
  if(!input.right_shift && !input.control) {
    state.directions = input.directions;
    state.accelerate = input.accelerate;
    state.decelerate = input.decelerate;
  }
  state.o_held = input.o;
}

} // namespace tfx::flight
