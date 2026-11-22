#include <cstdlib>
#include <iostream>
#include <utility>
#include "cockpit/mfd.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"usage: mfd_probe DATA_DIRECTORY [--pages]"};
  bool const controls{argc == 3 && std::string_view{argv[2]} == "--controls"};
  if(argc == 3 && std::string_view{argv[2]} == "--pages") {
    for(unsigned int side{0}; side != 2; ++side) {
      for(unsigned int page{0}; page != 9; ++page) std::cout << std::to_underlying(tfx::cockpit::outer_page(static_cast<tfx::cockpit::display_side>(side), page)) << '\n';
    }
    return EXIT_SUCCESS;
  }
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::mfd const display{tables.instruments(), game, tables.weapons};
  unsigned int page{0};
  unsigned int side{0};
  unsigned int aircraft{0};
  while(std::cin >> page >> side >> aircraft) {
    tfx::cockpit::mfd_state state;
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
    std::cin >> state.speed >> state.height >> state.heading >> state.fuel >> state.fuel_gauge >> state.smoothed_fuel_burn >> state.throttle >> state.offset.x >> state.offset.y >> state.engine_load[0] >> state.engine_load[1];
    for(auto &condition : state.systems) {
      unsigned int value{0};
      std::cin >> value;
      condition = static_cast<tfx::flight::system_condition>(value);
    }
    tfx::weapons::armament inventory{state.aircraft, tables.weapons, tables.arming};
    std::cin >> inventory.selected >> inventory.guns >> state.confirmed_target;
    for(auto &pylon : inventory.pylons) {
      unsigned int weapon{0};
      unsigned int count{0};
      std::cin >> weapon >> count;
      pylon.weapon = static_cast<uint8_t>(weapon);
      pylon.count = static_cast<uint8_t>(count);
    }
    state.inventory = &inventory;
    if(controls) {
      unsigned int mode{0};
      unsigned int configuration{0};
      auto &values{state.controls};
      auto &autopilot{values.autopilot};
      std::cin >> values.trailing_edge_flap >> values.leading_edge_flap >> values.thrust_vector.x >> values.thrust_vector.y >> autopilot.enabled >> mode >> configuration >> autopilot.heading >> autopilot.waypoint >> autopilot.height >> autopilot.speed;
      autopilot.mode = static_cast<tfx::cockpit::autopilot_mode>(mode);
      values.configuration = static_cast<tfx::cockpit::flight_configuration>(configuration);
    }
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    display.draw(renderer, static_cast<tfx::cockpit::display_side>(side), static_cast<tfx::cockpit::display_page>(page), state, &coverage);
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
