#include <bit>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "flight/stores.h"
#include "resources/executable_tables.h"

auto main(int const argc, char const *const argv[])->int try {
  bool const inventory{argc == 3 && std::string_view{argv[1]} == "--inventory"};
  if(argc != 1 && !inventory) throw std::runtime_error{"stores_probe [--inventory DATA_DIRECTORY]"};
  std::unique_ptr<tfx::resources::executable_tables> tables;
  if(inventory) tables = std::make_unique<tfx::resources::executable_tables>(std::filesystem::path{argv[2]} / "TFX.DAT");
  unsigned int aircraft{0};
  while(std::cin >> aircraft) {
    tfx::flight::airframe_load airframe;
    airframe.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft);
    std::cin >> airframe.moment_a >> airframe.moment_b >> airframe.fuel_capacity;
    tfx::flight::loadout stores;
    if(inventory) {
      tfx::weapons::armament weapons{airframe.aircraft, tables->weapons, tables->arming};
      std::array<vec3d, tfx::flight::store_slots> mounts;
      for(unsigned int slot{0}; slot != mounts.size(); ++slot) {
        unsigned int weapon{0};
        unsigned int count{0};
        if(!(std::cin >> mounts[slot].x >> mounts[slot].y >> mounts[slot].z >> weapon >> count)) throw std::runtime_error{"truncated inventory replay"};
        weapons.pylons[slot].weapon = static_cast<uint8_t>(weapon);
        weapons.pylons[slot].count = static_cast<uint8_t>(count);
      }
      stores = weapons.flight_stores(mounts);
    } else {
      for(auto &store : stores) {
        int mass{0};
        unsigned int drag{0};
        unsigned int tank{0};
        unsigned int occupied{0};
        if(!(std::cin >> store.position.x >> store.position.y >> store.position.z >> mass >> drag >> tank >> occupied)) throw std::runtime_error{"truncated loadout replay"};
        store.properties.mass_kg = static_cast<int16_t>(mass);
        store.properties.drag = std::bit_cast<float>(drag);
        store.properties.fuel_tank = tank != 0;
        store.occupied = occupied != 0;
      }
    }
    auto const result{tfx::flight::accumulate_stores(airframe, stores)};
    for(auto const value : {result.drag, result.mass_kg, result.moment_a, result.moment_b, result.total_moment_a, result.total_moment_b, result.fuel_capacity}) std::cout << std::bit_cast<uint64_t>(value) << ' ';
    std::cout << result.fuel_tank << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
