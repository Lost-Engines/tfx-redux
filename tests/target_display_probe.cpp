#include <cstdlib>
#include <iostream>
#include "cockpit/target_display.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: target_display_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::target_display const display{tables.instruments()};
  unsigned int mode{0};
  unsigned int sight{0};
  unsigned int name{0};
  tfx::cockpit::target_display_state state;
  tfx::cockpit::air_target_reading target;
  vec2<int> minimum{0, 0};
  vec2<int> maximum{319, 199};
  while(std::cin >> mode >> sight >> name >> state.player.x >> state.player.y >> state.player.z >> target.position_q8.x >> target.position_q8.y >> target.position_q8.z >> target.heading >> target.speed_q16 >> state.projected.x >> state.projected.y >> state.tracking_q8.x >> state.tracking_q8.y >> state.offset.x >> state.offset.y) {
    for(unsigned int row{0}; row != 3; ++row) {
      for(auto *axis : {&state.orientation.right, &state.orientation.down, &state.orientation.forward}) std::cin >> (*axis)[row];
    }
    std::cin >> minimum.x >> minimum.y >> maximum.x >> maximum.y;
    state.sight = static_cast<tfx::cockpit::target_sight>(sight);
    std::array<std::string_view, 7> constexpr names{"MiG-29 Fulcrum", "F-117", "Efa Eurofighter", "AIR AEW", "C-130 tanker", "SAM site", "E3D"};
    target.name = names.at(name);
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    renderer.clipping = tfx::render::raster_clip::viewport(minimum, maximum);
    if(mode == 0) {
      display.information(renderer, target, state, &coverage);
    } else if(mode == 1) {
      display.direction(renderer, target, state, minimum, maximum, &coverage);
    } else if(mode == 2) {
      display.edge_marker(renderer, state, &coverage);
    } else if(mode == 5) {
      display.ground(renderer, target.position_q8, state, minimum, maximum, &coverage);
    } else {
      display.marker(renderer, state.sight, mode == 4, {state.projected.x, state.projected.y}, &coverage);
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
