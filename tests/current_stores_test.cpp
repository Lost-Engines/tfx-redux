#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "weapons/loadout.h"

namespace {
auto require(bool const condition, char const *const message)->void {
  if(!condition) throw std::runtime_error{message};
}
} // anonymous namespace

int main() try {
  using tfx::weapons::weapon_id;
  std::vector<tfx::weapons::definition> definitions(tfx::weapons::weapon_count);
  auto &missile{definitions[std::to_underlying(weapon_id::amraam)]};
  missile.mass = 100;missile.drag = .25f;
  auto &tank{definitions[std::to_underlying(weapon_id::fuel_tank)]};
  tank.mass = 200;tank.drag = .5f;tank.role = tfx::weapons::weapon_role::external_tank;
  tfx::weapons::arming_tables tables;
  for(unsigned int slot{0}; slot != tables.next.size(); ++slot) tables.next[slot] = static_cast<uint16_t>((slot + 1) % tables.next.size());
  tfx::weapons::armament weapons{tfx::flight::player_aircraft::eurofighter,definitions,tables};
  std::array<vec3d,tfx::flight::store_slots> mounts;
  mounts[0] = {1,2,3};
  weapons.assign(0,weapon_id::amraam);
  auto carried{weapons.flight_stores(mounts)};
  require(carried[0].occupied && carried[0].position == mounts[0] && carried[0].properties.mass_kg == 100 && carried[0].properties.drag == .25f && !carried[0].properties.fuel_tank,"initial carried weapon metadata differs");
  weapons.assign(0,weapon_id::fuel_tank);
  carried = weapons.flight_stores(mounts);
  require(carried[0].occupied && carried[0].properties.mass_kg == 200 && carried[0].properties.drag == .5f && carried[0].properties.fuel_tank,"replacing a store retained the old mass/drag/tank role");
  weapons.pylons[0].count = 3;
  require(weapons.flight_stores(mounts)[0].properties.mass_kg == 200,"native quantity is an occupancy gate, not a mass multiplier");
  weapons.pylons[0].count = 0;
  require(weapons.pylons[0].visible && !weapons.flight_stores(mounts)[0].occupied,"depleted visible dispensers must not contribute store mass");
  weapons.assign(0,std::nullopt);
  require(!weapons.flight_stores(mounts)[0].occupied,"empty pylon became a carried weapon");
  std::cout << "Current inventory to flight stores: replaced weapon metadata, mounts, quantity and depleted/empty slots passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
