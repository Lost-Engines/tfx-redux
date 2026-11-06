#include "aerodynamic_pilot_keys.h"
#include <utility>

namespace tfx::flight {
namespace {
using calculation = double;
uint16_t constexpr waypoint_mode{0}, heading_mode{1}, track_mode{2}, autothrottle_mode{3}, recovery_mode{4};
float constexpr feet_per_world_unit{8}, metres_per_foot{.30480000376701355f};
double captured_height(float const height) { return static_cast<double>(static_cast<calculation>(height) * feet_per_world_unit * metres_per_foot); }
} // anonymous namespace

aerodynamic_pilot_key_events press_aerodynamic_pilot_key(aerodynamic_flight &state, damage_state &damage, bool &stick_pressure, aerodynamic_pilot_key const key, aerodynamic_pilot_key_context const &context) {
  aerodynamic_pilot_key_events events;
  auto &targets{state.manual.controls.autopilot};
  switch(key) {
    case aerodynamic_pilot_key::cruise_speed:
      targets.metres_per_second = static_cast<double>(static_cast<calculation>(state.aircraft.cruise_mach) * state.data.atmosphere.sound_metres_per_second);
      return events;
    case aerodynamic_pilot_key::stick_pressure:
      stick_pressure = !stick_pressure;
      events.notice = stick_pressure ? aerodynamic_pilot_notice::pressure_on : aerodynamic_pilot_notice::pressure_off;
      return events;
    case aerodynamic_pilot_key::waypoint:
      targets.mode = waypoint_mode;
      if(targets.enabled) {
        targets.down_metres = captured_height(state.view.position.y);
        events.notice = aerodynamic_pilot_notice::waypoint;
      }
      return events;
    case aerodynamic_pilot_key::heading:
      targets.mode = heading_mode;
      targets.heading = state.view.angles.y;
      targets.down_metres = state.response.motion.pose.metres_ned.z;
      if(targets.enabled) events.notice = aerodynamic_pilot_notice::heading;
      return events;
    case aerodynamic_pilot_key::track:
      targets.mode = track_mode;
      if(targets.enabled) events.notice = aerodynamic_pilot_notice::track;
      return events;
    case aerodynamic_pilot_key::autothrottle:
      targets.mode = autothrottle_mode;
      if(targets.enabled) events.notice = aerodynamic_pilot_notice::autothrottle;
      return events;
    case aerodynamic_pilot_key::toggle_autopilot: break;
  }
  if(context.inhibited || state.response.motion.collision || context.ejected || state.ground.ground.landed) return events;
  auto &air_brake{damage.systems[std::to_underlying(aircraft_system::air_brake)]};
  state.manual.air_brake_condition = air_brake;
  if(targets.mode == recovery_mode) {
    events.recovery = toggle_aerodynamic_recovery(state.autopilot.recovery,state.manual.controls,state.powerplant,air_brake,{
      .afterburner_stages{state.aircraft.powerplant.afterburner_stages},
      .available{context.recovery_available},
      .inhibited{context.inhibited},
      .crashed{state.response.motion.collision != 0},
      .ejected{context.ejected},
      .landed{state.ground.ground.landed},
    });
    state.manual.air_brake_condition = air_brake;
    return events;
  }
  if(!targets.enabled) {
    events.external_status = autopilot_external_status::on;
    if(targets.metres_per_second == 0) targets.metres_per_second = state.data.metres_per_second;
    if(targets.mode == heading_mode) {
      if(state.model == aerodynamic_model::realistic) state.data.track_heading = math::signed_word(state.view.angles.y);
      targets.heading = static_cast<math::angle>(state.data.track_heading);
      targets.down_metres = captured_height(state.view.position.y);
    }
    if(targets.mode == waypoint_mode) targets.down_metres = captured_height(state.view.position.y);
    switch(targets.mode) {
      case waypoint_mode: events.notice = aerodynamic_pilot_notice::waypoint; break;
      case heading_mode: events.notice = aerodynamic_pilot_notice::heading; break;
      case track_mode: events.notice = aerodynamic_pilot_notice::tracking; break;
      case autothrottle_mode: events.notice = aerodynamic_pilot_notice::autothrottle; break;
      default: events.notice = aerodynamic_pilot_notice::engaged; break;
    }
    targets.enabled = true;
    state.autopilot.saved_air_brake = air_brake;
  } else {
    events.external_status = autopilot_external_status::off;
    targets.enabled = false;
    air_brake = state.autopilot.saved_air_brake;
    state.manual.air_brake_condition = air_brake;
  }
  state.reset_motion(context.ground_elevation_steps,context.terrain_present);
  events.motion_reset = true;
  return events;
}

} // namespace tfx::flight
