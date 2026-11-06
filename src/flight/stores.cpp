#include "stores.h"

namespace tfx::flight {

store_effects accumulate_stores(airframe_load const &airframe, loadout const &stores) {
  // Preserve the original slot order and round each contribution to double.
  double constexpr external_tank_multiplier{2};
  store_effects result;
  for(auto const &store : stores) {
    if(!store.occupied) continue;
    auto const &definition{store.properties};
    vec3d const offset{store.position};
    vec2d const plane_a{offset.x,offset.y}, plane_b{offset.x,offset.z};
    result.drag += definition.drag;
    result.moment_a += plane_a.length_sq() * definition.mass_kg;
    result.moment_b += plane_b.length_sq() * definition.mass_kg;
    result.mass_kg += definition.mass_kg;
    result.fuel_tank = result.fuel_tank || definition.fuel_tank;
  }
  result.total_moment_a = airframe.moment_a + result.moment_a;
  result.total_moment_b = airframe.moment_b + result.moment_b;
  result.fuel_capacity = airframe.fuel_capacity;
  if(airframe.aircraft == player_aircraft::eurofighter && result.fuel_tank) result.fuel_capacity *= external_tank_multiplier;
  return result;
}

} // namespace tfx::flight
