#include <cstdlib>
#include <iomanip>
#include <iostream>
#include "cockpit/radio_ticker.h"
#include "cockpit/readouts.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"usage: radio_probe DATA_DIRECTORY [frames]"};
  bool const frames{argc == 3};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::readouts const painter{tables.instruments(), game};
  tfx::cockpit::radio_ticker ticker{tables.instruments().radio_padding};
  unsigned int action{0};
  unsigned int ticks{0};
  unsigned int warp{0};
  unsigned int view{0};
  vec2<int16_t> offset{0, 0};
  std::string message;
  while(std::cin >> action >> ticks >> warp >> view >> offset.x >> offset.y >> std::quoted(message)) {
    if(action == 1 || action == 2) {
      ticker.show(message, action == 2);
    } else if(action == 3) {
      ticker.clear();
    } else if(action == 4) {
      ticker.replay();
    } else if(action == 5) ticker.stop();
    auto const window{ticker.advance(static_cast<tfx::simulation::timer_ticks>(ticks), warp != 0)};
    auto const state{ticker.status()};
    if(frames) {
      std::array<int32_t, 6> const header{state.cursor, state.remainder, state.complete, state.history, state.persistent, window.has_value()};
      renderer.output.pixels.fill(99);
      tfx::render::framebuffer coverage;
      renderer.clipping = tfx::render::raster_clip::viewport({0, 0}, {319, 199});
      if(window) painter.draw_radio(renderer, *window, view == 0, offset, &coverage);
      std::cout.write(reinterpret_cast<char const *>(header.data()), sizeof(header));
      std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
      std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
    } else {
      std::cout << state.cursor << ' ' << state.remainder << ' ' << state.complete << ' ' << state.history << ' ' << state.persistent << ' ' << window.has_value() << ' ' << std::quoted(window.value_or("")) << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
