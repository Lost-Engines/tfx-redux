#include <cstdlib>
#include <iostream>
#include "cockpit/warnings.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: warning_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::cockpit::warning_lamps lamps{tables.instruments(), game};
  unsigned int first{0};
  while(std::cin >> first) {
    tfx::cockpit::warning_state state;
    state.systems[0] = static_cast<tfx::flight::system_condition>(first);
    for(unsigned int index{1}; index != state.systems.size(); ++index) {
      unsigned int condition{0};
      std::cin >> condition;
      state.systems[index] = static_cast<tfx::flight::system_condition>(condition);
    }
    int category{-1};
    std::cin >> state.autopilot >> state.hook >> state.fire_permission_notice >> category >> state.stall_recovery >> state.threat >> state.offset.x >> state.offset.y;
    if(category >= 0) state.tracked_weapon_category = static_cast<uint8_t>(category);
    tfx::render::framebuffer image;
    tfx::render::framebuffer coverage;
    image.pixels.fill(99);
    bool const sound{lamps.draw(image, state, &coverage)};
    for(auto const value : {lamps.state().countdown, lamps.state().phase}) {
      std::cout.put(static_cast<char>(value));
      std::cout.put(static_cast<char>(value >> 8));
    }
    std::cout.put(static_cast<char>(sound));
    std::cout.write(reinterpret_cast<char const *>(image.pixels.data()), image.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
