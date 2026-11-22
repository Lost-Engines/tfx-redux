#pragma once

#include <cstdint>

namespace tfx::flight {

enum class cheat_key {
  none,
  p,
  l,
  o,
  other
};
struct cheat_state {
  bool enabled{false};
  uint8_t matched{0};
};

// Original right-Shift PLOP dispatcher. Matching makes are consumed before
// ordinary commands (in particular P pause and O weapon selection).
class cheat_code {
private:
  cheat_state state;

public:
  auto press(cheat_key key, bool right_shift)->bool;
  auto value() const->cheat_state const &;
};

} // namespace tfx::flight
