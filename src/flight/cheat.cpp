#include "cheat.h"
#include <array>

namespace tfx::flight {

auto cheat_code::press(cheat_key const key, bool const right_shift)->bool {
  if(!right_shift || key == cheat_key::none) return false;
  std::array constexpr sequence{cheat_key::p, cheat_key::l, cheat_key::o, cheat_key::p};
  if(key != sequence[state.matched]) {
    state.matched = 0;
    return false;
  }
  if(++state.matched == sequence.size()) {
    state.matched = 0;
    state.enabled = !state.enabled;
  }
  return true;
}
auto cheat_code::value() const->cheat_state const & {
  return state;
}

} // namespace tfx::flight
