#include "radio_ticker.h"
#include <stdexcept>
#include "math/native.h"

namespace tfx::cockpit {

radio_ticker::radio_ticker(std::string_view const spaces) : padding{spaces} {
}
void radio_ticker::clear() {
  // Native 0x81174 leaves the cursor and fractional timing remainder alone.
  complete = true;history = false;persistent = false;buffer.clear();
}
void radio_ticker::show(std::string_view message, bool const repeat) {
  // Native 0x81198/0x81254 pads both ends, retaining only uppercase letters
  // and digits. Lowercase and punctuation become spaces rather than glyphs.
  message = message.substr(0,message.find('\0'));
  buffer = padding;buffer += message;buffer += padding;
  for(char &character : buffer) if((character < 'A' || character > 'Z') && (character < '0' || character > '9')) character = ' ';
  cursor = 0;complete = false;history = true;persistent = repeat;
}
void radio_ticker::replay() {
  // Native 0x81268 can replay the last message after its first pass finishes.
  if(history) { complete = false;cursor = 0; }
}
void radio_ticker::stop() {
  complete = true;
}
radio_status radio_ticker::status() const {
  return {
    .cursor{cursor},
    .remainder{remainder},
    .complete{complete},
    .history{history},
    .persistent{persistent},
  };
}
std::optional<std::string> radio_ticker::advance(simulation::timer_ticks const elapsed, bool const time_warp) {
  // Native callers skip 0x81290 once complete. It copies the current window
  // before advancing the cursor, and suppresses the final partial window.
  if(complete) return {};
  if(cursor < 0 || static_cast<unsigned int>(cursor) > buffer.size()) throw std::out_of_range{"radio cursor exceeds its message"};
  unsigned int constexpr window_characters{19};
  auto window{buffer.substr(static_cast<unsigned int>(cursor),window_characters)};
  if(window.size() < window_characters || static_cast<unsigned int>(cursor) + window_characters == buffer.size()) {
    complete = !persistent;
    if(persistent) cursor = 0;
    return {};
  }
  int constexpr ordinary_ticks_per_character{4}, warped_ticks_per_character{20};
  int const divisor{time_warp ? warped_ticks_per_character : ordinary_ticks_per_character};
  int const total{remainder + elapsed}, advance{total / divisor};
  cursor = math::signed_word(static_cast<uint16_t>(cursor + advance));
  remainder = math::signed_word(static_cast<uint16_t>(total - advance * divisor));
  return window;
}

} // namespace tfx::cockpit
