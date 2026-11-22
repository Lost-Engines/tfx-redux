#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "flight/keyboard.h"

namespace {
auto require(bool const condition, char const *const message)->void {
  if(!condition) throw std::runtime_error{message};
}
} // anonymous namespace

int main() try {
  tfx::flight::keyboard_state state;
  tfx::flight::poll_keyboard(state,{
    .directions{
      .up{true},
    },
    .accelerate{true},
  });
  require(state.directions.up && state.accelerate,"ordinary keys did not refresh manual controls");
  tfx::flight::poll_keyboard(state,{
    .directions{
      .down{true},
    },
    .decelerate{true},
    .o{true},
    .right_shift{true},
  });
  require(state.directions.up && !state.directions.down && state.accelerate && !state.decelerate,"right Shift did not retain the prior manual flags");
  require(state.o_held && state.view_pan == 8,"O or right-Shift panning was gated incorrectly");
  state.target_pan = 0xc0;
  tfx::flight::poll_keyboard(state,{
    .directions{
      .up{true},
      .right{true},
    },
    .page_down{true},
    .control{true},
  });
  require(state.view_pan == 0 && state.target_pan == 0xd6,"Ctrl translations or retained high bits changed");
  require(state.accelerate && !state.o_held,"Ctrl must retain manual flags while refreshing O");
  tfx::flight::poll_keyboard(state,{
    .o{true},
    .automatic_landing{true},
  });
  require(state.accelerate && !state.o_held && state.target_pan == 0xc0,"automatic landing must retain manual/O flags, but release pan flags");
  tfx::flight::poll_keyboard(state,{});
  require(!state.accelerate && !state.directions.up,"unmodified release must refresh manual controls");
  std::cout << "Original held keyboard: modifier retention, independent O, orbit/translation packing and automatic landing passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
