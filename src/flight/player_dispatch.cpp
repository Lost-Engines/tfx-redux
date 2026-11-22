#include "player_dispatch.h"

namespace tfx::flight {

player_dispatch select_player_callback(player_dispatch_context const &context) {
  if(context.pilot_active) {
    return {
      .callback{player_callback::pilot},
      .clear_autopilot{true},
      .clear_automatic_landing{true},
    };
  }
  if(context.aircraft_disabled) return {.callback{player_callback::disabled_aircraft}};
  if(context.automatic_landing) return {.callback{player_callback::automatic_landing}};
  switch(context.model) {
  case simulation::flight_model::military_spec:
    return {.callback{player_callback::military_spec}};
  case simulation::flight_model::realistic:
    return {.callback{player_callback::realistic}};
  case simulation::flight_model::arcade:
  case simulation::flight_model::simple:
    if(context.autopilot) return {.callback{context.elapsed > context.autopilot_military_tick_limit ? player_callback::realistic : player_callback::military_spec}};
    return {.callback{context.model == simulation::flight_model::arcade ? player_callback::arcade : player_callback::simple}};
  default:
    return {};
  }
}

} // namespace tfx::flight
