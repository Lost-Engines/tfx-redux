#include <cstdlib>
#include <iostream>
#include "flight/keyboard.h"

int main() {
  tfx::flight::keyboard_state state;
  unsigned int mask;
  while(std::cin >> mask) {
    auto const held{[&](unsigned int const bit){return (mask & (1u << bit)) != 0;}};
    tfx::flight::poll_keyboard(state, {
      .directions{
        .up{held(0)},
        .left{held(1)},
        .down{held(2)},
        .right{held(3)},
      },
      .home{held(4)},
      .end{held(5)},
      .page_up{held(6)},
      .page_down{held(7)},
      .accelerate{held(8)},
      .decelerate{held(9)},
      .o{held(10)},
      .right_shift{held(11)},
      .control{held(12)},
      .automatic_landing{held(13)},
    });
    std::cout << state.directions.up << ' ' << state.directions.left << ' ' << state.directions.down << ' ' << state.directions.right << ' ' << state.accelerate << ' ' << state.decelerate << ' ' << state.o_held << ' ' << static_cast<unsigned int>(state.view_pan) << ' ' << static_cast<unsigned int>(state.target_pan) << '\n';
  }
  return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
}
