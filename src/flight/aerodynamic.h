#pragma once

#include "flight/aerodynamic_approach_reference.h"
#include "flight/aerodynamic_autopilot.h"
#include "flight/aerodynamic_commands.h"
#include "flight/aerodynamic_realistic_ground.h"
#include "flight/aerodynamic_response.h"
#include "flight/aerodynamic_start.h"
#include "flight/aerodynamic_wind.h"
#include "simulation/timing.h"

namespace tfx::flight {

using aerodynamic_model = simulation::flight_model;                            // Configured native mode, distinct from the callback selected for this update.

enum class input_configuration : uint16_t {
  combat,
  cruise
};

struct aerodynamic_input_gains {
  double pitch{1};
  double roll{.19};
  double rudder{.03};
  double joystick{1};
};

struct aerodynamic_flight {
  aerodynamic_airframe aircraft{};
  aerodynamic_coefficient_parameters coefficient_parameters{};
  aerodynamic_coefficients coefficients{};
  aerodynamic_airdata data{};
  aerodynamic_manual_state manual{};
  aerodynamic_powerplant powerplant{};
  aerodynamic_surface_state surfaces{};
  aerodynamic_response_state response{};
  aerodynamic_realistic_ground_state ground{};
  aerodynamic_force_result forces{};
  aerodynamic_commands commands{};
  aerodynamic_autopilot autopilot{};
  aerodynamic_approach_reference approach{};
  aerodynamic_wind wind{};
  loadout mounted_stores{};
  store_effects stores{};
  aerodynamic_view view{};
  aerodynamic_model model{aerodynamic_model::military_spec};
  aerodynamic_input_gains input_gains{};
  double seconds{.01};
  double gear_fraction{0};
  bool arrestor_hook_down{false};
  bool chute_deployed{false};
  bool chute_cutaway{false};
  bool cold_start{false};
  // Native weapon-mode keys also select the flight response and HUD configuration.
  auto configure_controls(input_configuration configuration)->void;
  // Native 0x954fc, also called immediately by the autopilot toggle key.
  void reset_motion(int16_t ground_elevation_steps, bool terrain_present);
};

struct aerodynamic_initial_context {
  aerodynamic_start_context placement{};
  aerodynamic_model model{aerodynamic_model::military_spec};
  aerodynamic_wind_context wind{};
  std::optional<int16_t> surface_celsius{};
};

// Fresh native 0x95f3c numerical state. Cockpit/UI, control-device acquisition,
// weather selection, campaign state and external output delivery remain callers.
aerodynamic_flight initialise_aerodynamic_flight(aerodynamic_airframe const &aircraft, loadout const &stores, aerodynamic_initial_context const &context, aerodynamic_wind_parameters const &wind_parameters, damage_state &damage, math::random_state &random);

} // namespace tfx::flight
