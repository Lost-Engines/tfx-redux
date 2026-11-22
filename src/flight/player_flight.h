#pragma once

#include "flight/aerodynamic_launch.h"
#include "flight/aerodynamic_step.h"
#include "flight/automatic_landing.h"
#include "flight/ejection_sequence.h"
#include "flight/fuel_gauge.h"
#include "flight/player_dispatch.h"
#include "flight/simple.h"

namespace tfx::flight {

struct player_flight_context {
  landing_state const &runway;
  aerodynamic_tick_context physics{};
  flight_controls basic_controls{};
  simulation::timer_ticks elapsed{0};
  int16_t surface_type{-1};
  int16_t pilot_terrain_type{-1};
  double refuel_per_update{14};
  bool aircraft_disabled{false};
  bool water_world{false};
};
struct player_flight_events {
  std::optional<int16_t> horizon_motion{};
  player_callback callback{player_callback::none};
  aerodynamic_tick_events aerodynamic{};
  automatic_landing_events landing{};
  std::vector<unsigned int> model_entries{};
  std::vector<unsigned int> gear_sounds{};
  bool refuel_afterburner_warning{false};
  bool pilot_fatal{false};
  bool pilot_ended{false};
};

// One player owns both sets of retained flight-model caches. Its outer callback
// selects the numerical path and transfers only the globals shared by them.
// World actors, collision collection, camera and output delivery remain callers.
struct player_flight {
  simple_state basic{};
  aerodynamic_flight physical{};
  automatic_landing automatic{};
  ejection_sequence ejection{};
  fuel_gauge fuel{
    .reading{0},
    .smoothed_burn{2},
  };
  int32_t measured_speed_q8{0};
  bool simple_body{false};
  bool stick_pressure{true};

  void initialise(aerodynamic_airframe const &aircraft, loadout const &stores,
    aerodynamic_launch_context const &context, float basic_mach,
    aerodynamic_wind_parameters const &wind, damage_state &damage, math::random_state &random);
  void select_model(aerodynamic_model model);
  math::attitude const &body() const;
  player_flight_events advance(damage_state &damage, math::random_state &random,
    aerodynamic_wind_parameters const &wind, player_flight_context const &context);
};

} // namespace tfx::flight
