#include "defence.h"

namespace tfx::flight {

defensive_effects press_defensive_key(defensive_systems &state, defensive_key const key, defensive_context const &context) {
  defensive_effects result;
  // Native 0x85f50/0x86258 compare the zero-based aircraft index with 1:
  // the F-117 ignores both keys. Other craft cannot run ECM and stealth together.
  if(key == defensive_key::ecm || key == defensive_key::stealth) {
    if(context.aircraft == player_aircraft::nighthawk) return result;
    if(key == defensive_key::ecm) {
      state.ecm = !state.ecm;
      if(state.ecm) state.stealth = false;
    } else {
      state.stealth = !state.stealth;
      if(state.stealth) {
        state.ecm = false;
        result.clear_targeting = true;
      }
    }
    return result;
  }
  // Native 0x86124/0x86168 spend a cartridge before requesting the separately owned projectile population.
  if(context.ejecting || context.collision || context.disabled) return result;
  bool const radar{key == defensive_key::chaff};
  auto &remaining{radar ? state.chaff : state.flares};
  if(!remaining) return result;
  --remaining;
  int16_t constexpr radar_strength{60};
  int16_t constexpr flare_strength{40};
  int16_t constexpr afterburning_flare_strength{5};
  result.release.emplace(countermeasure_release{
    .radar{radar},
    .strength{radar ? radar_strength : context.afterburner ? afterburning_flare_strength : flare_strength},
  });
  return result;
}

} // namespace tfx::flight
