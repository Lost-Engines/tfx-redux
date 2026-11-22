#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "world/collision.h"

int main() try {
  tfx::world::collision_box const box{
    .minimum{-10, -10, -10},
    .maximum{10, 10, 10},
  };
  struct sample {
    tfx::world::collision_segment movement;
    bool axis_aligned;
    bool diagonal;
  };
  std::array<sample, 8> const samples{{
    {
      .movement{
        .previous{0, 0, 0},
        .current{0, 0, 0}
      },
      .axis_aligned{true},
      .diagonal{true}
    },
    {
      .movement{
        .previous{20, 0, 0},
        .current{20, 0, 0}
      },
      .axis_aligned{false},
      .diagonal{false}
    },
    {
      .movement{
        .previous{-20, 0, 0},
        .current{20, 0, 0}
      },
      .axis_aligned{true},
      .diagonal{true}
    },
    {
      .movement{
        .previous{-20, 0, 0},
        .current{0, 0, -20}
      },
      .axis_aligned{false},
      .diagonal{false}
    },
    {
      .movement{
        .previous{0, 0, -20},
        .current{-20, 0, 0}
      },
      .axis_aligned{true},
      .diagonal{false}
    },
    {
      .movement{
        .previous{20, 0, 20},
        .current{-20, 0, -20}
      },
      .axis_aligned{true},
      .diagonal{true}
    },
    {
      .movement{
        .previous{0, 0, 0},
        .current{20, 0, 20}
      },
      .axis_aligned{true},
      .diagonal{true}
    },
    {
      .movement{
        .previous{-32768, -32768, -32768},
        .current{32767, 32767, 32767}
      },
      .axis_aligned{true},
      .diagonal{true}
    },
  }};
  for(auto const &test : samples) {
    if(tfx::world::intersects(box, test.movement) != test.axis_aligned || tfx::world::intersects(box, test.movement, true) != test.diagonal) throw std::runtime_error{"native swept-box boundary differs"};
  }
  std::cout << "Native collision boundaries passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
