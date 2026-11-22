#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "render/palette_selection.h"

int main(int const argc, char const *const argv[]) {
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open native palette fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::render::palette_selection selection;
  unsigned int reset;
  unsigned int dirty;
  unsigned int infrared;
  unsigned int view;
  unsigned int projectiles;
  unsigned int available;
  unsigned int night;
  unsigned int night_vision;
  while(input >> reset >> dirty >> infrared >> view >> projectiles >> available >> night >> night_vision) {
    if(reset) {
      selection = tfx::render::palette_selection{{
        .normal_changed{dirty != 0},
        .infrared{infrared != 0},
      }};
    } else if(dirty) selection.invalidate_normal();
    auto const upload{selection.advance({
      .view{static_cast<tfx::cockpit::view_mode>(view)},
      .projectiles_present{projectiles != 0},
      .infrared_available{available != 0},
      .night{night != 0},
      .night_vision{night_vision != 0},
    })};
    int const request{upload ? std::to_underlying(*upload) : -1};
    if(argc == 2) {
      int expected_request;
      int expected_dirty;
      int expected_infrared;
      if(!(input >> expected_request >> expected_dirty >> expected_infrared) || request != expected_request || selection.value().normal_changed != (expected_dirty != 0) || selection.value().infrared != (expected_infrared != 0)) throw std::runtime_error{"native palette-selection fixture differs"};
    } else {
      std::cout << request << ' ' << selection.value().normal_changed << ' ' << selection.value().infrared << '\n';
    }
  }
}
