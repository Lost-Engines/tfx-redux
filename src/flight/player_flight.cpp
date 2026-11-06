#include "player_flight.h"
#include <utility>
#include "flight/player_frame.h"
#include "flight/player_shared.h"
#include "flight/gear_controls.h"

namespace tfx::flight {
namespace {
using calculation = double;
float constexpr normalised_word{3.0518509447574615e-05f}, metres_per_foot{.30480000376701355f}, feet_per_world_unit{8};
double constexpr minimum_descent{.1}, autopilot_recovery_seconds{8}, urgent_recovery_seconds{5}, warning_seconds{7};

void basic_recovery(player_flight &player, damage_state &damage, player_flight_context const &context, aerodynamic_tick_events &events) {
  auto &physical{player.physical};
  auto &targets{physical.manual.controls.autopilot};
  if(targets.mode == 4 || physical.gear_fraction != 0) return;
  // Native 0x8f72c uses the BASIC speed cache, even when the active callback
  // is aerodynamic. Fresh advanced-only flight leaves that float at zero.
  calculation const descent{static_cast<calculation>(math::signed_word(static_cast<uint16_t>(player.body().forward.y))) * normalised_word * player.basic.flight.speed_metres_per_second};
  if(!(descent > minimum_descent)) return;
  double const seconds{static_cast<double>(static_cast<calculation>(player.basic.flight.position.y) * metres_per_foot * feet_per_world_unit / -descent)};
  if(!(seconds > 0)) return;
  if((seconds < autopilot_recovery_seconds && targets.enabled) || seconds < urgent_recovery_seconds) {
    auto &air_brake{damage.systems[std::to_underlying(aircraft_system::air_brake)]};
    auto const recovery{toggle_aerodynamic_recovery(physical.autopilot.recovery,physical.manual.controls,physical.powerplant,air_brake,{
      .afterburner_stages{physical.aircraft.powerplant.afterburner_stages},
      .available{context.physics.recovery_available},
      .inhibited{context.physics.recovery_inhibited},
      .crashed{physical.response.motion.collision != 0},
      .ejected{context.aircraft_disabled},
      .landed{physical.ground.ground.landed},
    })};
    if(recovery.change != aerodynamic_recovery_change::none) events.recovery_changes.push_back(recovery.change);
    events.afterburner.insert(events.afterburner.end(),recovery.afterburner.begin(),recovery.afterburner.end());
    physical.manual.air_brake_condition = air_brake;
  } else events.descent_warning = seconds < warning_seconds;
}
} // anonymous namespace

void player_flight::initialise(aerodynamic_airframe const &aircraft, loadout const &stores,
    aerodynamic_launch_context const &context, float const basic_mach,
    aerodynamic_wind_parameters const &wind, damage_state &damage, math::random_state &random) {
  basic = {};
  basic.flight.aircraft = aircraft.stores_and_mounts.stores.aircraft;
  ground_contact const ground{
    .present{true},
    .height{context.ground_elevation_steps},
  };
  if(context.model == aerodynamic_model::simple) initialise_simple(basic,context.position,context.angles,basic_mach,context.landed,ground);
  else initialise_arcade(basic.flight,context.position,context.angles,basic_mach,context.landed,ground);
  basic.reported_speed = context.landed ? 0 : .5f;
  physical = launch_aerodynamic_flight(aircraft,stores,context,wind,damage,random);
  // Native 6C3C is shared with the cockpit gauge and survives initialization.
  // An immediate automatic approach skips the usual fuel-burn conversion.
  physical.powerplant.fuel_readout = fuel.reading;
  publish_shared_player(basic,physical);
  basic.ground.body = physical.view.orientation;
  basic.ground.visible = physical.view.orientation;
  simple_body = context.model == aerodynamic_model::simple;
  automatic = {};
  ejection.reset();
  measured_speed_q8 = 0;
}

void player_flight::select_model(aerodynamic_model const model) {
  if(physical.model == model) return;
  if(!simple_body) basic.ground.body = basic.flight.orientation; // Native shared body matrix also receives Arcade exports.
  physical.model = model;
  physical.autopilot.recovery.model_reset_requested = true;
  basic.first_update = true;
}

math::attitude const &player_flight::body() const {
  if(ejection.value().stage == ejection_stage::pilot) return ejection.value().pilot.orientation;
  return simple_body ? basic.ground.body : basic.flight.orientation;
}

player_flight_events player_flight::advance(damage_state &damage, math::random_state &random,
    aerodynamic_wind_parameters const &wind, player_flight_context const &context) {
  player_flight_events events;
  auto &view{basic.flight};
  bool const ejected{context.aircraft_disabled};
  capture_shared_player(physical,basic,damage,simple_body);
  if(physical.cold_start) {
    place_aerodynamic_start(physical,context.physics.ground_elevation_steps);
    publish_shared_player(basic,physical);
  }
  vec3f previous_position{view.position};
  if(context.physics.refuelling_phase) {
    events.refuel_afterburner_warning = physical.powerplant.afterburner != 0;
    physical.powerplant.fuel = static_cast<double>(static_cast<calculation>(physical.powerplant.fuel) + context.refuel_per_update);
    if(physical.powerplant.fuel > physical.stores.fuel_capacity) physical.powerplant.fuel = physical.stores.fuel_capacity;
    view.fuel = physical.powerplant.fuel;
  }
  basic_recovery(*this,damage,context,events.aerodynamic);
  auto &gear_condition{damage.systems[std::to_underlying(aircraft_system::landing_gear)]};
  if(view.aircraft == player_aircraft::eurofighter && physical.gear_fraction != 0 && view.fixed_speed > eurofighter_maximum_gear_speed && automatic.phase == automatic_landing_phase::inactive && !context.physics.recovery_inhibited && !view.collision && !ejected && gear_condition != system_condition::failed) {
    physical.gear_fraction = 0;
    view.gear_down = false;
    gear_condition = system_condition::normal;
    events.model_entries.push_back(4);
  }
  if(view.landed && view.fixed_speed < 5 && !context.physics.recovery_inhibited && !view.collision && !ejected && view.aircraft != player_aircraft::raptor && view.brake_chute) {
    physical.chute_deployed = view.brake_chute = false;
    physical.chute_cutaway = view.brake_chute_cut_away = true;
    events.model_entries.push_back(16);
  }
  physical.seconds = context.physics.seconds; // The outer timer writes this even for a basic callback.
  update_aerodynamic_engine_faults(physical.powerplant,random);
  damage.systems[std::to_underlying(aircraft_system::left_engine)] = physical.powerplant.engine_condition[0];
  damage.systems[std::to_underlying(aircraft_system::right_engine)] = physical.powerplant.engine_condition[1];
  view.left_engine = physical.powerplant.engine_factor.x != 0;
  view.right_engine = physical.powerplant.engine_factor.y != 0;
  vec2<system_condition> const engine_conditions{physical.powerplant.engine_condition[0],physical.powerplant.engine_condition[1]};
  auto const dispatch{select_player_callback({
    .model{physical.model},
    .elapsed{context.elapsed},
    .autopilot{physical.manual.controls.autopilot.enabled},
    .automatic_landing{automatic.phase != automatic_landing_phase::inactive},
    .aircraft_disabled{ejected},
    .pilot_active{ejection.value().stage == ejection_stage::pilot},
  })};
  events.callback = dispatch.callback;
  if(dispatch.clear_autopilot) physical.manual.controls.autopilot.enabled = false;
  if(dispatch.clear_automatic_landing) automatic.phase = automatic_landing_phase::inactive;
  ground_contact const ground{
    .present{context.physics.terrain_present},
    .height{context.physics.ground_elevation_steps},
  };
  bool physical_callback{false};
  switch(dispatch.callback) {
    case player_callback::pilot: {
      ejection.collision(view.collision);
      auto const update{ejection.advance_pilot({
        .elapsed{context.elapsed},
        .ground_height{ground.present ? ground.height : int16_t{0}},
        .terrain_type{context.pilot_terrain_type},
        .water_world{context.water_world},
      })};
      events.model_entries.insert(events.model_entries.end(),update.model_entries.begin(),update.model_entries.end());
      auto const &pilot{ejection.value().pilot};
      view.position = vec3f{pilot.position};
      view.integer_position = vec3<int32_t>{pilot.integer_position};
      view.orientation = pilot.orientation;
      view.angles = vec3<math::angle>{pilot.angles};
      view.collision = pilot.collision;
      simple_body = false;
      events.pilot_fatal = pilot.fatal;
      events.pilot_ended = pilot.ended;
      break;
    }
    case player_callback::automatic_landing:
      // The scripted approach and route controller reuse these exact globals.
      // Import the retained route values before setup consumes the old position.
      automatic.approach.destination_metres = vec3<int32_t>{physical.autopilot.navigation.target_position};
      automatic.approach.player_metres = vec3<int32_t>{physical.autopilot.navigation.player_position};
      automatic.approach.distance_km = physical.autopilot.navigation.route_distance;
      automatic.approach.pitch_degrees = physical.autopilot.guidance.heading_error;
      physical.seconds = context.physics.seconds;
      events.landing = advance_automatic_landing(automatic,physical,damage,{
        .runway{context.runway},
        .ground_elevation_steps{ground.height},
        .inhibited{context.physics.recovery_inhibited},
        .ejected{ejected},
      });
      if(events.landing.clear_basic_speed) {
        view.speed_mach = 0;
        basic.reported_speed = 0; // Native 0x13440, distinct from the target-Mach cache at 0x133cc.
      }
      physical_callback = true;
      break;
    case player_callback::military_spec:
    case player_callback::realistic: {
      auto physics{context.physics};
      physics.callback = dispatch.callback == player_callback::realistic ? aerodynamic_callback::realistic : aerodynamic_callback::military;
      auto tick{advance_aerodynamic_flight(physical,damage,random,wind,physics)};
      events.aerodynamic.recovery_changes.insert(events.aerodynamic.recovery_changes.end(),tick.recovery_changes.begin(),tick.recovery_changes.end());
      events.aerodynamic.afterburner.insert(events.aerodynamic.afterburner.end(),tick.afterburner.begin(),tick.afterburner.end());
      events.aerodynamic.descent_warning = events.aerodynamic.descent_warning || tick.descent_warning;
      events.aerodynamic.ground = tick.ground;
      physical_callback = true;
      break;
    }
    case player_callback::disabled_aircraft:
      basic.displayed_g_load = 0; // Native disabled-callback tail 0x8e7fd.
      if(physical.model == aerodynamic_model::arcade) advance_disabled(view,context.basic_controls,engine_conditions,context.elapsed,ground);
      else {
        basic.gear.condition = gear_condition;
        advance_simple_disabled(basic,context.basic_controls,engine_conditions,context.elapsed,ground);
      }
      break;
    case player_callback::simple: {
      basic.gear.condition = gear_condition;
      auto update{advance_simple(basic,{
        .controls{context.basic_controls},
        .source{context.physics.input.source},
        .keys{context.physics.input.keys},
        .joystick{vec2<int16_t>{context.physics.input.joystick}},
        .alt{context.physics.input.alt},
        .surface_type{context.surface_type},
      },engine_conditions,context.elapsed,ground)};
      events.gear_sounds = std::move(update.gear_sounds);
      events.horizon_motion = update.horizon_motion;
      gear_condition = basic.gear.condition;
      simple_body = true;
      break;
    }
    case player_callback::arcade: {
      // Simple keeps its visible stall/ground offsets apart from the shared
      // body matrix. Arcade reuses the body, not that display-only attitude.
      if(simple_body) view.orientation = basic.ground.body;
      if(physical.autopilot.recovery.model_reset_requested && view.landed) view.position.y = static_cast<float>(static_cast<calculation>(ground.height) - basic_parameters(view.aircraft).gear_height);
      auto const update{advance_arcade(view,context.basic_controls,engine_conditions,context.elapsed,ground,context.physics.time_warp)};
      previous_position = update.movement_origin;
      events.horizon_motion = update.horizon_motion;
      simple_body = false;
      break;
    }
    case player_callback::none: break;
  }
  if(physical_callback) {
    publish_shared_player(basic,physical);
    simple_body = false;
  } else capture_shared_player(physical,basic,damage,simple_body);
  measured_speed_q8 = finish_player_frame(physical,damage,fuel,{
    .previous_position{previous_position},
    .elapsed{context.elapsed},
    .ground_elevation_steps{ground.height},
    .terrain_present{ground.present},
  });
  view.position.y = physical.view.position.y;
  view.collision = physical.response.motion.collision;
  view.fuel_readout = physical.powerplant.fuel_readout;
  basic.first_update = false;
  return events;
}

} // namespace tfx::flight
