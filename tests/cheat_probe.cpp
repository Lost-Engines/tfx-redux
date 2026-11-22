#include <cstdlib>
#include <iostream>
#include <utility>
#include "flight/cheat.h"

int main() {
  tfx::flight::cheat_code code;
  unsigned int key;
  bool right_shift;
  while(std::cin >> key >> right_shift) {
    if(key > std::to_underlying(tfx::flight::cheat_key::other)) return EXIT_FAILURE;
    bool const consumed{code.press(static_cast<tfx::flight::cheat_key>(key), right_shift)};
    std::cout << consumed << ' ' << code.value().enabled << ' ' << static_cast<unsigned int>(code.value().matched) << '\n';
  }
  return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
}
