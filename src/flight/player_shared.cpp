#include "player_shared.h"
#include <utility>

namespace tfx::flight {

void capture_shared_player(aerodynamic_flight &physical, simple_state const &basic, damage_state const &damage, bool const simple_body) {
  auto const &view{basic.flight};
  physical.view.position = vec3f{view.position};
  physical.view.integer_position = vec3<int32_t>{view.integer_position};
  physical.view.angles = vec3<math::angle>{view.angles};
  physical.view.orientation = simple_body ? basic.ground.body : view.orientation;
  physical.data.fixed_speed = view.fixed_speed;
  physical.response.g_load = basic.displayed_g_load;
  physical.data.mach = view.unrounded_speed_mach;
  physical.data.angle_of_attack = basic.ground.indicated_speed;                // Shared native scratch 0x14418.
  physical.approach.flight_path_degrees = basic.ground.low_speed_reference;    // Shared 0x14290.
  physical.surfaces.stalled = view.recovering;
  physical.response.motion.collision = view.collision;
  physical.response.motion.integer_world_y = view.integer_position.y;
  physical.response.low_speed_ground_contact = view.low_speed_ground_contact;
  physical.ground.ground.landed = view.landed;
  if(!view.arrested) {
    physical.ground.ground.arrestor = aerodynamic_arrestor_phase::free;
  } else if(physical.ground.ground.arrestor == aerodynamic_arrestor_phase::free) physical.ground.ground.arrestor = aerodynamic_arrestor_phase::engaged;
  physical.gear_fraction = view.gear_down ? 1 : 0;
  physical.arrestor_hook_down = view.arrestor_hook;
  physical.chute_deployed = view.brake_chute;
  physical.chute_cutaway = view.brake_chute_cut_away;
  auto &engine{physical.powerplant};
  engine.throttle = view.throttle;
  engine.afterburner = view.afterburner;
  engine.fuel = view.fuel;
  engine.fuel_capacity = view.fuel_capacity;
  engine.extra_fuel_capacity = view.extra_fuel_capacity;
  engine.fuel_flow = view.fuel_flow;
  engine.fuel_readout = view.fuel_readout;
  // The UI's engine switches express on/off, while these factors also retain
  // partial power loss. Preserve a nonzero factor when the switch stays on.
  if(!view.left_engine) {
    engine.engine_factor.x = 0;
  } else if(engine.engine_factor.x == 0) engine.engine_factor.x = 1;
  if(!view.right_engine) {
    engine.engine_factor.y = 0;
  } else if(engine.engine_factor.y == 0) engine.engine_factor.y = 1;
  engine.engine_condition = {damage.condition(aircraft_system::left_engine), damage.condition(aircraft_system::right_engine)};
  physical.manual.air_brake_condition = damage.condition(aircraft_system::air_brake);
}

void publish_shared_player(simple_state &basic, aerodynamic_flight const &physical) {
  auto &view{basic.flight};
  view.position = vec3f{physical.view.position};
  view.integer_position = vec3<int32_t>{physical.view.integer_position};
  view.angles = vec3<math::angle>{physical.view.angles};
  view.orientation = physical.view.orientation;
  basic.ground.body = physical.view.orientation;
  view.fixed_speed = physical.data.fixed_speed;
  view.unrounded_speed_mach = physical.data.mach;
  basic.ground.indicated_speed = physical.data.angle_of_attack;
  basic.ground.low_speed_reference = physical.approach.flight_path_degrees;
  view.recovering = physical.surfaces.stalled;
  view.collision = physical.response.motion.collision;
  view.low_speed_ground_contact = physical.response.low_speed_ground_contact;
  view.landed = physical.ground.ground.landed;
  view.arrested = physical.ground.ground.arrestor != aerodynamic_arrestor_phase::free;
  view.gear_down = physical.gear_fraction != 0;
  view.arrestor_hook = physical.arrestor_hook_down;
  view.brake_chute = physical.chute_deployed;
  view.brake_chute_cut_away = physical.chute_cutaway;
  view.air_brake = physical.manual.air_brake_condition == system_condition::active;
  auto const &engine{physical.powerplant};
  view.throttle = engine.throttle;
  view.afterburner = engine.afterburner;
  view.fuel = engine.fuel;
  view.fuel_capacity = engine.fuel_capacity;
  view.extra_fuel_capacity = engine.extra_fuel_capacity;
  view.fuel_flow = engine.fuel_flow;
  view.fuel_readout = engine.fuel_readout;
  view.left_engine = engine.engine_factor.x != 0;
  view.right_engine = engine.engine_factor.y != 0;
  basic.displayed_g_load = physical.response.g_load;
}

} // namespace tfx::flight
