#include <iostream>
#include <stdexcept>
#include "cockpit/external_display.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"external display probe requires original data directory"};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::external_display const display{tables.instruments(), tables.presentation_fonts()};
  tfx::cockpit::external_view_state state;
  unsigned int background;
  while(std::cin >> state.height >> state.speed >> state.heading >> state.enabled >> background) {
    renderer.output.pixels.fill(static_cast<uint8_t>(background));
    tfx::render::framebuffer coverage;
    display.draw(renderer, state, &coverage);
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
}
