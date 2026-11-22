#include <cstdlib>
#include <iostream>
#include "cockpit/auxiliary.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: auxiliary_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::cockpit::auxiliary_lamps lamps{tables.instruments(), game};
  unsigned int left{0};
  unsigned int right{0};
  unsigned int redraw{0};
  int cue{-1};
  while(std::cin >> left >> right >> redraw) {
    tfx::cockpit::auxiliary_state state;
    state.engines = {static_cast<tfx::flight::system_condition>(left), static_cast<tfx::flight::system_condition>(right)};
    state.redraw_phase = static_cast<uint8_t>(redraw);
    std::cin >> state.afterburner_pips >> cue >> state.approach_suppressed >> state.low_speed_ground_contact >> state.landed >> state.offset.x >> state.offset.y;
    if(cue >= 0) state.approach_cue = static_cast<uint8_t>(cue);
    tfx::render::framebuffer image;
    tfx::render::framebuffer coverage;
    image.pixels.fill(99);
    lamps.draw(image, state, &coverage);
    std::cout.put(static_cast<char>(lamps.state().engine_refresh));
    std::cout.put(static_cast<char>(lamps.state().afterburner_refresh));
    std::cout.write(reinterpret_cast<char const *>(image.pixels.data()), image.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
