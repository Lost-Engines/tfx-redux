#include <cstdlib>
#include <iostream>
#include "cockpit/radar.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: radar_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::radar radar{tables.instruments(), game};
  unsigned int mode{0};
  unsigned int range{0};
  unsigned int aircraft{0};
  unsigned int condition{0};
  unsigned int role{0};
  while(std::cin >> mode >> range) {
    tfx::cockpit::radar_state state;
    tfx::math::random_state random;
    std::cin >> state.player.x >> state.player.y >> state.player.z >> state.heading >> aircraft >> condition >> state.stealth >> state.guns >> role >> state.offset.x >> state.offset.y >> random.low >> random.high;
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
    state.condition = static_cast<tfx::flight::system_condition>(condition);
    state.selected_role = static_cast<tfx::weapons::weapon_role>(role);
    std::array<std::vector<tfx::cockpit::radar_aircraft>, 2> air;
    std::array<std::vector<tfx::cockpit::radar_surface>, 2> surface;
    for(auto &list : air) {
      unsigned int count{0};
      std::cin >> count;
      list.resize(count);
      for(auto &contact : list) std::cin >> contact.position_q8.x >> contact.position_q8.y >> contact.position_q8.z >> contact.selected;
    }
    for(auto &list : surface) {
      unsigned int count{0};
      std::cin >> count;
      list.resize(count);
      for(auto &contact : list) std::cin >> contact.position_q8.x >> contact.position_q8.y >> contact.position_q8.z >> contact.type;
    }
    std::vector<tfx::cockpit::radar_projectile> shots;
    unsigned int count{0};
    std::cin >> count;
    shots.resize(count);
    for(auto &shot : shots) {
      unsigned int weapon{0};
      std::cin >> shot.position_q8.x >> shot.position_q8.y >> shot.position_q8.z >> weapon >> shot.player_owned;
      shot.weapon = static_cast<tfx::weapons::weapon_id>(weapon);
    }
    state.enemy_aircraft = air[0];
    state.allied_aircraft = air[1];
    state.enemy_surface = surface[0];
    state.allied_surface = surface[1];
    state.projectiles = shots;
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    radar.draw(renderer, static_cast<tfx::cockpit::radar_mode>(mode), range, state, random, &coverage);
    std::cout.put(static_cast<char>(radar.phase()));
    for(auto const value : {random.low, random.high}) {
      std::cout.put(static_cast<char>(value));
      std::cout.put(static_cast<char>(value >> 8));
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
