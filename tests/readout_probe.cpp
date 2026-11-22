#include <cstdlib>
#include <iostream>
#include <utility>
#include "cockpit/readouts.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"usage: readout_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::readouts const instruments{tables.instruments(), game};
  tfx::cockpit::readout_state state;
  if(argc == 3 && std::string_view{argv[2]} == "--autopilot") {
    tfx::cockpit::autopilot_display_state autopilot;
    unsigned int mode{0};
    while(std::cin >> autopilot.enabled >> mode >> state.offset.x >> state.offset.y) {
      autopilot.mode = static_cast<tfx::cockpit::autopilot_mode>(mode);
      renderer.output.pixels.fill(99);
      tfx::render::framebuffer coverage;
      instruments.draw_autopilot(renderer, autopilot, state.offset, &coverage);
      std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
      std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
    }
    return EXIT_SUCCESS;
  }
  unsigned int aircraft{0};
  unsigned int chaff{0};
  unsigned int flares{0};
  while(std::cin >> state.elapsed_ticks >> state.clock_offset >> state.fuel >> state.throttle >> aircraft >> state.ecm >> state.stealth >> chaff >> flares >> state.offset.x >> state.offset.y) {
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
    state.chaff = static_cast<uint8_t>(chaff);
    state.flares = static_cast<uint8_t>(flares);
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    auto const status{instruments.draw(renderer, state, &coverage)};
    std::cout.put(static_cast<char>(std::to_underlying(status)));
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
