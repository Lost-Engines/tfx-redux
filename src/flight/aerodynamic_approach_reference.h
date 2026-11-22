#pragma once

#include "flight/aerodynamic_airdata.h"
#include "flight/aerodynamic_surfaces.h"
#include "flight/damage.h"

namespace tfx::flight {

struct aerodynamic_approach_reference {
  double flap_fraction{0};
  double lift_coefficient{0};
  double angle_of_attack{0};
  double flight_path_degrees{0};
  double ground_speed{0};                                                      // Shared native ground/pitch-rate scratch.
};
struct aerodynamic_approach_reference_context {
  aerodynamic_airdata const &data;
  aerodynamic_motion const &motion;
  aerodynamic_coefficients const &coefficients;
  aerodynamic_surface_parameters const &geometry;
  player_aircraft aircraft{player_aircraft::eurofighter};
  double low_speed_reference{0};
  system_condition flaps_condition{system_condition::normal};
};

// Native 0x99a30, enabled by ILS: reference flap/lift/alpha and current flight
// path. These are reference quantities, not immediate control-surface changes.
void update_aerodynamic_approach_reference(aerodynamic_approach_reference &state, aerodynamic_approach_reference_context const &context);

} // namespace tfx::flight
