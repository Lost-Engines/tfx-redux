#pragma once

#include "flight/aircraft.h"
#include "flight/damage.h"

namespace tfx::flight {

struct aerodynamic_powerplant_parameters {
  player_aircraft aircraft{player_aircraft::eurofighter};
  double dry_thrust{0};
  double afterburner_thrust{0};
  uint8_t afterburner_stages{0};
  double ceiling_km{20.3};
  double dry_fuel_coefficient{0};
  double afterburner_fuel_coefficient{0};
  double roll_thrust_arm{0};
  double pitch_thrust_arm{0};
};
enum class afterburner_change {
  none,
  started,
  increased,
  decreased,
  stopped
};
struct aerodynamic_powerplant {
  vec2d engine_factor{1, 1};
  std::array<system_condition, 2> engine_condition{system_condition::normal, system_condition::normal};
  vec2d nozzle_angles{0, 0};
  vec3d engine_force{0, 0, 0};
  vec3d engine_moment{0, 0, 0};
  float throttle{0};
  double thrust{0};
  double throttle_target{0};
  double fuel{0};
  double fuel_capacity{0};
  double extra_fuel_capacity{0};
  double fuel_flow{0};
  uint8_t afterburner{0};
  int16_t fuel_readout{0};
  uint8_t afterburner_control{0};
  uint8_t afterburner_display{0};
  bool stop_afterburner();
  afterburner_change increase_afterburner(unsigned int stages);
  afterburner_change decrease_afterburner();
};
struct aerodynamic_powerplant_context {
  double altitude_km{0};
  double seconds{.02};
  bool time_warp{false};
};

// Native 0x9cc54: throttle, afterburner, altitude, engine availability and
// F-22 thrust-vectoring loads. Retained unmodified load axes remain explicit.
void update_aerodynamic_thrust(aerodynamic_powerplant &state, aerodynamic_powerplant_parameters const &parameters, aerodynamic_powerplant_context const &context);
// Native 0x99424. True means the original afterburner-stop notifications fire.
bool consume_aerodynamic_fuel(aerodynamic_powerplant &state, aerodynamic_powerplant_parameters const &parameters, aerodynamic_powerplant_context const &context);
// Native 0x995e0: ordered per-engine intermittent decay and random consumption.
void update_aerodynamic_engine_faults(aerodynamic_powerplant &state, math::random_state &random);

} // namespace tfx::flight
