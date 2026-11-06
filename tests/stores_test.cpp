#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "flight/stores.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}

} // anonymous namespace

auto main()->int try {
  tfx::flight::loadout stores;
  stores[0] = {
    .position{1,2,3},
    .properties{
      .mass_kg{100},
      .drag{0.25f},
      .fuel_tank{true},
    },
    .occupied{true},
  };
  tfx::flight::airframe_load airframe{
    .aircraft{tfx::flight::player_aircraft::eurofighter},
    .moment_a{1000},
    .moment_b{2000},
    .fuel_capacity{100},
  };
  auto effects{tfx::flight::accumulate_stores(airframe,stores)};
  require(effects.mass_kg == 100 && effects.drag == 0.25,"store mass/drag contribution differs");
  require(effects.moment_a == 500 && effects.moment_b == 1000,"mounted-store moment contributions differ");
  require(effects.total_moment_a == 1500 && effects.total_moment_b == 3000,"airframe plus store moments differ");
  require(effects.fuel_capacity == 200 && effects.fuel_tank,"Eurofighter external-tank capacity differs");
  airframe.aircraft = tfx::flight::player_aircraft::nighthawk;
  effects = tfx::flight::accumulate_stores(airframe,stores);
  require(effects.fuel_capacity == 100 && effects.fuel_tank,"external-tank capacity multiplier is aircraft-specific");
  stores[0].occupied = false;
  effects = tfx::flight::accumulate_stores(airframe,stores);
  require(effects.mass_kg == 0 && effects.drag == 0 && !effects.fuel_tank && effects.total_moment_a == 1000,"unoccupied stores must not contribute");
  std::cout << "Native store mass, drag, moments, fuel capacity and occupancy checks passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
