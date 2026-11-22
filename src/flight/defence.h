#pragma once

#include <cstdint>
#include <optional>
#include "flight/aircraft.h"

namespace tfx::flight {

enum class defensive_key {
  ecm,
  stealth,
  chaff,
  flare
};
struct defensive_systems {
  uint8_t chaff{0};
  uint8_t flares{0};
  bool ecm{false};
  bool stealth{false};
};
struct defensive_context {
  player_aircraft aircraft;
  bool ejecting{false};
  bool collision{false};
  bool disabled{false};
  uint8_t afterburner{0};
};
struct countermeasure_release {
  bool radar;
  int16_t strength;
};
struct defensive_effects {
  bool clear_targeting{false};
  std::optional<countermeasure_release> release{};
};

defensive_effects press_defensive_key(defensive_systems &state, defensive_key key, defensive_context const &context);

} // namespace tfx::flight
