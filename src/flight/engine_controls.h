#pragma once

#include <optional>
#include <vector>
#include "flight/damage.h"

namespace tfx::flight {

enum class engine_side {
  left,
  right
};
enum class engine_key_event {
  stop_afterburner,
  stop_sample,
  start_samples,
  start_synth,
  stop_synth
};
struct engine_key_context {
  engine_side side;
  system_condition condition;
  system_condition other_condition;
  bool afterburner;
  bool inhibited;
  bool crashed;
  bool disabled;
};
struct engine_key_effects {
  std::optional<bool> running;
  std::vector<engine_key_event> ordered;
};
// Native 842B0/84370: failed/intermittent engines cannot be toggled. The two
// handlers deliberately retain their different recorded/synth start order.
engine_key_effects press_engine_key(engine_key_context const &context);

} // namespace tfx::flight
