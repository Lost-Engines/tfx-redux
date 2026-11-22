#pragma once

#include "flight/aerodynamic_forces.h"
#include "flight/atmosphere.h"

namespace tfx::flight {

struct aerodynamic_airdata {
  aerodynamic_basis basis{};
  atmosphere_sample atmosphere{};
  double flight_path_pitch_degrees{0};
  double altitude_km{0};
  double angle_of_attack{0};
  double metres_per_second{0};
  double mach{0};
  double dynamic_pressure{0};
  double mass_kg{0};
  double simulation_seconds{0};
  int32_t fixed_speed{0};
  int32_t track_heading{0};
};
struct aerodynamic_airdata_context {
  vec3f wind_ned{0, 0, 0};
  double empty_mass_kg{0};
  double fuel_kg{0};
  double stores_kg{0};
  double seconds{.02};
  atmosphere_parameters atmosphere{};
  std::optional<int16_t> surface_celsius{};
};

// Native 0x98ebc..0x991cc: refresh physical axes, body/air velocities,
// track/attack angles, atmospheric values, mass, speed, Mach and pressure.
// Wind generation and the following coefficient/fuel stages are separate owners.
void update_aerodynamic_airdata(aerodynamic_airdata &state, aerodynamic_motion &motion, aerodynamic_airdata_context const &context);

} // namespace tfx::flight
