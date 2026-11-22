#pragma once

#include <vector>
#include "flight/aerodynamic.h"
#include "flight/automatic_approach.h"
#include "flight/landing.h"

namespace tfx::flight {

enum class automatic_landing_phase : uint8_t { inactive, prepare, approach };
enum class automatic_landing_event { afterburner_stopped, gear_lowered, gear_raised, stopped };
struct automatic_landing {
  automatic_approach approach{};
  automatic_landing_phase phase{automatic_landing_phase::inactive};
};
struct automatic_landing_context {
  landing_state const &runway;
  int16_t ground_elevation_steps{0};
  double resting_pitch_radians{0};                                             // Native presets 0x13eb8 are zero for all three aircraft.
  bool inhibited{false};
  bool ejected{false};
};
struct automatic_landing_events {
  std::vector<automatic_landing_event> ordered{};
  int32_t score_change{0};
  bool clear_basic_speed{false};                                               // Setup also clears the separate basic Mach and reported-speed caches.
};

// Complete native 0x9feb4, including setup 0xa0494, scripted approach,
// touchdown finalizer 0xa02c8, gear handler and final view export. The caller
// selects this callback while automatic landing is active, instead of physics.
automatic_landing_events advance_automatic_landing(automatic_landing &landing, aerodynamic_flight &flight, damage_state &damage, automatic_landing_context const &context);

} // namespace tfx::flight
