#include <cstdlib>
#include <iostream>
#include "cockpit/attitude_display.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: attitude_display_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::attitude_display const display{tables.instruments(), game};
  tfx::cockpit::attitude_display_state state;
  unsigned int mode{0};
  while(std::cin >> mode >> state.angles.x >> state.angles.y >> state.angles.z >> state.projection_angles.x >> state.projection_angles.y >> state.projection_origin.x >> state.projection_origin.y >> state.offset.x >> state.offset.y) {
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    if(mode == 0) {
      display.draw_heading(renderer, state, &coverage);
    } else {
      display.draw_pitch(renderer, state, &coverage);
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
