#include <cstdlib>
#include <iostream>
#include "cockpit/hud_text.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"usage: hud_text_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::hud_text const text{tables.instruments()};
  tfx::cockpit::hud_text_state state;
  if(argc == 3) {
    tfx::cockpit::aerodynamic_hud_state flight;
    while(std::cin >> flight.angle_of_attack >> flight.down_metres_per_second >> state.offset.x >> state.offset.y) {
      renderer.output.pixels.fill(99);
      tfx::render::framebuffer coverage;
      text.draw_aerodynamic(renderer, flight, state.offset, &coverage);
      std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
      std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
    }
    return EXIT_SUCCESS;
  }
  while(std::cin >> state.height >> state.speed >> state.mach >> state.load >> state.throttle >> state.position.x >> state.position.y >> state.position.z >> state.air_brake >> state.brake_chute >> state.arrester_cable >> state.offset.x >> state.offset.y) {
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    text.draw(renderer, state, &coverage);
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
