#pragma once

#include <optional>
#include "flight/aircraft.h"
#include "flight/damage.h"

namespace tfx::flight {

inline int32_t constexpr eurofighter_maximum_gear_speed{350};
struct gear_key_context {
  player_aircraft aircraft;
  int32_t speed;
  bool gear_down;
  system_condition condition;
  bool inhibited;
  bool crashed;
  bool disabled;
  bool automatic_landing;
  bool landed;
};
struct gear_key_effects {
  bool speed_warning{false};
  std::optional<bool> gear_down;
};
// Native 84914/84990: speed warning precedes the ordinary eligibility gates.
gear_key_effects press_gear_key(gear_key_context const &context);

} // namespace tfx::flight
