#include "initialisation.h"
#include <utility>

namespace tfx::cockpit {

instrument_setup initialise_instruments(flight::player_aircraft const aircraft, bool const landed) {
  // Native 0x7df30 owns these startup settings; the executable's stored bytes
  // are overwritten here and are not the actual mission-start defaults.
  bool const nighthawk{aircraft == flight::player_aircraft::nighthawk};
  int16_t constexpr full_fuel_gauge{9999};
  int16_t constexpr initial_fuel_burn{10};
  uint8_t constexpr initial_countermeasures{15};
  instrument_setup result{
    .displays{
      .left{2},
      .centre{nighthawk ? centre_display::moving_map : centre_display::horizontal_radar},
      .right{static_cast<uint16_t>(nighthawk ? 4 : 0)},
      .horizontal_range{3},
      .bvr_range{2},
    },
    .offset{0, 28},
    .systems{},
    .fuel{
      .reading{full_fuel_gauge},
      .smoothed_burn{initial_fuel_burn},
    },
    .chaff{initial_countermeasures},
    .flares{initial_countermeasures},
    .stealth{nighthawk},
  };
  result.systems.fill(flight::system_condition::normal);
  for(auto const engine : {flight::aircraft_system::left_engine, flight::aircraft_system::right_engine}) result.systems[std::to_underlying(engine)] = landed ? flight::system_condition::normal : flight::system_condition::active;
  if(landed) result.systems[std::to_underlying(flight::aircraft_system::landing_gear)] = flight::system_condition::active;
  return result;
}

} // namespace tfx::cockpit
