#include <cstdlib>
#include <iostream>
#include "cockpit/weapon_display.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"usage: weapon_display_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::weapon_display display{tables.instruments(), game};
  tfx::cockpit::weapon_display_state state;
  unsigned int mode{0};
  unsigned int aircraft{0};
  unsigned int weapon{0};
  while(std::cin >> mode >> aircraft >> weapon >> state.ammunition >> state.suppressed >> state.air_lock >> state.ground_lock >> state.limited >> state.attitude.angles.x >> state.attitude.angles.y >> state.attitude.angles.z >> state.attitude.projection_angles.x >> state.attitude.projection_angles.y >> state.attitude.projection_origin.y >> state.attitude.offset.x >> state.attitude.offset.y) {
    for(auto &point : state.cannon_path) std::cin >> point.x >> point.y;
    state.mode = static_cast<tfx::cockpit::aiming_mode>(mode);
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
    state.cannon = weapon == 0;
    state.weapon = tables.weapons.at(weapon).label;
    state.range = tables.weapons.at(weapon).range;
    state.category = tables.weapons.at(weapon).category;
    state.bomb_impact.reset();
    if(tables.weapons.at(weapon).category == 9) state.bomb_impact.emplace(vec2<int16_t>{state.cannon_path[0]});
    state.flight.offset = vec2<int16_t>{state.attitude.offset};
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    renderer.clipping = mode == 0 ? tfx::render::raster_clip::viewport({0, 0}, {319, 199}) : tfx::render::raster_clip::viewport({91 + state.attitude.offset.x, 4 + state.attitude.offset.y}, {231 + state.attitude.offset.x, 97 + state.attitude.offset.y});
    if(argc == 3) {
      display.draw_forward(renderer, state, &coverage);
    } else {
      display.draw(renderer, state, &coverage);
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
