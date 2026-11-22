#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "flight/cheat.h"

namespace {
auto require(bool const condition, char const *const message)->void {
  if(!condition) throw std::runtime_error{message};
}
} // anonymous namespace

int main() try {
  using tfx::flight::cheat_key;
  tfx::flight::cheat_code code;
  require(!code.press(cheat_key::p,false) && code.value().matched == 0,"ordinary P must reach pause");
  require(code.press(cheat_key::p,true) && code.value().matched == 1,"first P must be consumed");
  require(!code.press(cheat_key::none,true) && code.value().matched == 1,"idle polling must retain the partial sequence");
  require(!code.press(cheat_key::other,false) && code.value().matched == 1,"releasing right Shift must retain progress");
  require(code.press(cheat_key::l,true) && code.press(cheat_key::o,true) && code.press(cheat_key::p,true),"matching cheat letters escaped to commands");
  require(code.value().enabled && code.value().matched == 0,"completed sequence did not toggle and reset");
  require(code.press(cheat_key::p,true) && !code.press(cheat_key::p,true),"mismatched P must pass to pause without restarting");
  require(code.value().enabled && code.value().matched == 0,"mismatch changed the cheat setting");
  require(code.press(cheat_key::p,true) && code.press(cheat_key::l,true) && code.press(cheat_key::o,true) && code.press(cheat_key::p,true),"second sequence was not consumed");
  require(!code.value().enabled && code.value().matched == 0,"second complete sequence did not disengage");
  std::cout << "Original right-Shift cheat: consumption, idle/release retention, mismatch and repeated toggle passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
