#include <cstdlib>
#include <iostream>
#include <utility>
#include "presentation/arming_controller.h"
#include "presentation/arming_preview.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2) throw std::runtime_error{"usage: arming_controls_probe DATA_DIRECTORY [--hits]"};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  tfx::weapons::armament inventory{tfx::flight::player_aircraft::eurofighter, tables.weapons, tables.arming};
  if(argc == 3 && std::string_view{argv[2]} == "--motion") {
    int x{0};
    int y{0};
    int dx{0};
    int dy{0};
    while(std::cin >> x >> y >> dx >> dy) {
      tfx::presentation::arming_controller controller{inventory, tables.arming_art(), 0};
      controller.advance({100, 75}, {0, 0}, 1);
      controller.advance({100, 75}, {static_cast<int16_t>(dx), static_cast<int16_t>(dy)}, 0);
      auto const point{tfx::presentation::arming_preview::pointer_point({static_cast<int16_t>(x), static_cast<int16_t>(y)})};
      auto const &angles{controller.state().angles};
      std::cout << angles.x << ' ' << angles.y << ' ' << point.x << ' ' << point.y << ' ' << point.z << '\n';
    }
  } else if(argc == 3) {
    tfx::presentation::arming_controller controller{inventory, tables.arming_art(), 0};
    int x{0};
    int y{0};
    while(std::cin >> x >> y) {
      auto const region{controller.hit({static_cast<int16_t>(x), static_cast<int16_t>(y)})};
      std::cout << (region ? static_cast<int>(std::to_underlying(region->action)) : -1) << ' ' << (region ? region->row : 0) << '\n';
    }
  } else {
    unsigned int aircraft{0};
    unsigned int weapon{0};
    unsigned int slot{0};
    while(std::cin >> aircraft >> weapon >> slot) {
      tfx::weapons::armament stores{static_cast<tfx::flight::player_aircraft>(aircraft + 1), tables.weapons, tables.arming};
      tfx::presentation::arming_controller controller{stores, tables.arming_art(), 0};
      for(unsigned int mount{0}; mount != tfx::flight::store_slots; ++mount) {
        int id{0};
        std::cin >> id;
        stores.assign(mount, id < 0 ? std::nullopt : std::optional{static_cast<tfx::weapons::weapon_id>(id)});
      }
      auto const snapshot{[&]{
        for(unsigned int mount{0}; mount != tfx::flight::store_slots; ++mount) {
          auto const &p{stores.pylons[mount]};
          std::cout << static_cast<unsigned int>(p.weapon) << ' ' << static_cast<unsigned int>(p.count) << ' ' << p.visible << ' ' << controller.previews()[mount] << ' ';
        }
      }};
      controller.choose(static_cast<tfx::weapons::weapon_id>(weapon));
      std::cout << std::to_underlying(controller.state().mode) << ' ';
      snapshot();
      controller.place(slot);
      snapshot();
      controller.clear(slot);
      snapshot();
      controller.finish(true);
      snapshot();
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
