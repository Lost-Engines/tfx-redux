#include "math/floating_environment.h"
#include "simple.h"
#include "gear_controls.h"
#include "world/environment.h"
#include <cmath>
#include <limits>
#include <utility>

namespace tfx::flight {
namespace {

using calculation = long double;
float constexpr normalised_word{3.0518509447574615e-05f};
float constexpr fixed_speed_fraction{1.0f / 256}, speed_metres_per_native_tick{122};
float constexpr mach_per_metre_per_second{.0029411765281111f};
float constexpr geometric_metres_per_foot{.30480000376701355f}, kilometres_per_metre{.0010000000474974513f};
float constexpr fixed_speed_scale{256};
int32_t constexpr airborne_start_speed{250};
unsigned int constexpr normalisation_mask{31};
float constexpr collision_reset_height{-2}, collision_reset_position{-5};

int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0L) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void rotate(arcade_state &state, math::rotation_axis const axis, float const amount) {
  auto &remainder{state.rotation_remainder[static_cast<unsigned int>(axis)]};
  calculation const accumulated{static_cast<calculation>(remainder) + amount};
  remainder = static_cast<float>(accumulated);
  auto const rotation{static_cast<math::angle>(truncate(accumulated))};
  remainder = static_cast<float>(static_cast<calculation>(remainder) - math::signed_word(rotation));
  state.orientation.rotate(axis,rotation);
}

} // anonymous namespace

void initialise_simple(simple_state &state, vec3<int32_t> const &position, vec3<math::angle> const &angles, float const speed, bool const landed, ground_contact const ground) {
  math::floating_environment const environment;
  auto const aircraft{state.flight.aircraft};
  state = {};
  state.flight.aircraft = aircraft;
  initialise_arcade(state.flight,position,angles,speed,landed,ground);
  // Native engine/start setup 0x8c318 precedes the model-change conversion.
  state.flight.fixed_speed = landed ? 0 : airborne_start_speed;
  state.reported_speed = landed ? 0 : .5f;
  state.ground.body = state.flight.orientation;
  state.ground.visible = state.flight.orientation;
  state.ground.position_y = state.flight.position.y;
  state.ground.landed = landed;
  state.ground.low_speed_ground_contact = landed;
  state.gear.down = landed;
  state.gear.condition = landed ? system_condition::active : system_condition::normal;
}
void advance_simple_disabled(simple_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, ground_contact const ground) {
  math::floating_environment const environment;
  state.flight.orientation = state.ground.body;
  update_basic_disabled(state.flight,controls,engine_conditions,elapsed,ground,false,state.gear.condition == system_condition::active);
  state.ground.body = state.flight.orientation;
  state.displayed_g_load = 0;
  state.first_update = false;
}

simple_update advance_simple(simple_state &state, simple_inputs const &inputs, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, ground_contact const ground) {
  math::floating_environment const environment;
  auto &flight{state.flight};
  auto const &parameters{basic_parameters(flight.aircraft)};
  if(flight.aircraft == player_aircraft::eurofighter && flight.gear_down && flight.fixed_speed > eurofighter_maximum_gear_speed && !flight.collision && state.gear.condition != system_condition::failed) {
    flight.gear_down = false;
    state.gear.condition = system_condition::normal;
  }
  if(flight.landed && flight.fixed_speed < 5 && flight.brake_chute) { flight.brake_chute = false;flight.brake_chute_cut_away = true; }
  flight.orientation = state.ground.body;
  if(state.first_update) {
    calculation const movement{static_cast<calculation>(flight.fixed_speed) * fixed_speed_fraction};
    flight.movement_per_tick = static_cast<float>(movement);
    flight.speed_mach = static_cast<float>(movement * speed_metres_per_native_tick * mach_per_metre_per_second);
    if(flight.landed) flight.position.y = static_cast<float>(static_cast<calculation>(ground.height) - parameters.gear_height);
  }
  flight.recovering = false;
  state.geometric_altitude_km = static_cast<float>(-static_cast<calculation>(flight.position.y) * feet_per_world_unit * geometric_metres_per_foot * kilometres_per_metre);
  update_basic_speed(flight,inputs.controls,engine_conditions,elapsed,false,state.gear.condition == system_condition::active);
  state.turn.orientation = flight.orientation;
  state.turn.heading_remainder = flight.rotation_remainder.y;
  update_simple_turn(state.turn,{
    .mach{flight.speed_mach},
    .pitch_rate{state.controls.angular_rate.x},
    .elapsed{elapsed},
  });
  state.displayed_g_load = state.turn.g_load;
  flight.orientation = state.turn.orientation;
  flight.rotation_remainder.y = state.turn.heading_remainder;
  flight.movement_per_tick = state.turn.movement_per_tick;
  auto const keys{update_simple_controls(state.controls,{
    .fixed_speed{flight.fixed_speed},
    .metres_per_second{flight.speed_metres_per_second},
    .maximum_g{parameters.maximum_g},
    .minimum_g{parameters.minimum_g},
    .stall_metres_per_second{parameters.stall_metres_per_second},
    .coordinated_turn_rate{state.turn.coordinated_turn_rate},
    .orientation{flight.orientation},
    .elapsed{elapsed},
    .source{inputs.source},
    .joystick{vec2<int16_t>{inputs.joystick}},
    .keys{inputs.keys},
    .landed{flight.landed},
    .low_speed_ground_contact{flight.low_speed_ground_contact},
    .pitch_inhibited{flight.recovering},
    .alt{inputs.alt},
  })};
  consume_basic_fuel(flight,elapsed,false);
  rotate(flight,math::rotation_axis::pitch,static_cast<float>(static_cast<calculation>(elapsed) * state.controls.angular_rate.x));
  rotate(flight,math::rotation_axis::roll,static_cast<float>(static_cast<calculation>(elapsed) * state.controls.angular_rate.z));
  rotate(flight,math::rotation_axis::heading,static_cast<float>(static_cast<calculation>(elapsed) * state.controls.angular_rate.y));
  if(flight.normalisation_counter & normalisation_mask) flight.orientation.normalise();
  ++flight.normalisation_counter;
  state.ground.body = flight.orientation;
  state.ground.visible = flight.orientation;
  state.ground.rotation_remainder = vec3f{flight.rotation_remainder};
  state.ground.position_y = flight.position.y;
  state.ground.landed = flight.landed;
  state.ground.low_speed_ground_contact = flight.low_speed_ground_contact;
  state.ground.collision = flight.collision;
  state.ground.arrested = flight.arrested;
  simple_ground_context contact{
    .elapsed{elapsed},
    .ground{ground},
    .gear_height{parameters.gear_height},
    .metres_per_second{flight.speed_metres_per_second},
    .turn_metres_per_second{state.turn.metres_per_second},
    .reported_speed{state.reported_speed},
    .stall_metres_per_second{parameters.stall_metres_per_second},
    .low_speed_reference{parameters.low_speed_reference},
    .fixed_speed{flight.fixed_speed},
    .gear_down{flight.gear_down},
    .pitch_inhibited{flight.recovering},
    .pitch_down{keys.down},
    .surface_type{inputs.surface_type},
    .arrestor_hook{flight.arrestor_hook},
  };
  update_simple_stall_attitude(state.ground,contact);
  for(unsigned int axis{0}; axis != 3; ++axis) flight.position[axis] = static_cast<float>(static_cast<calculation>(math::signed_word(static_cast<uint16_t>(state.ground.body.forward[axis]))) * normalised_word * elapsed * flight.movement_per_tick + flight.position[axis]);
  flight.fixed_speed = truncate(static_cast<calculation>(flight.movement_per_tick) * fixed_speed_scale);
  state.gear.down = flight.gear_down;
  auto gear_sounds{update_landing_gear(state.gear,{
    .forward_y{math::signed_word(static_cast<uint16_t>(state.ground.body.forward.y))},
    .metres_per_second{flight.speed_metres_per_second},
    .landed{flight.landed},
    .previous_landed{state.ground.previous_landed},
    .airborne_speed_limit{parameters.gear_airborne_speed_limit},
    .ground_speed_limit{parameters.gear_ground_speed_limit},
    .touchdown_descent_limit{parameters.gear_touchdown_descent_limit},
  })};
  flight.gear_down = state.gear.down;
  state.ground.position_y = flight.position.y;
  contact.gear_down = flight.gear_down;
  contact.fixed_speed = flight.fixed_speed;
  update_simple_ground(state.ground,contact);
  flight.position.y = state.ground.position_y;
  flight.rotation_remainder = vec3f{state.ground.rotation_remainder};
  flight.orientation = state.ground.visible;
  flight.landed = state.ground.landed;
  flight.low_speed_ground_contact = state.ground.low_speed_ground_contact;
  flight.collision = state.ground.collision;
  flight.arrested = state.ground.arrested;
  if(!flight.crashing && flight.collision) {
    flight.collision = 0;
    if(flight.position.y > collision_reset_height) flight.position.y = collision_reset_position;
  }
  flight.angles = flight.orientation.angles();
  float constexpr world_extent{world::map_width * world::cell_units};
  for(auto *coordinate : {&flight.position.x,&flight.position.z}) {
    if(*coordinate < 0) *coordinate = static_cast<float>(static_cast<calculation>(*coordinate) + world_extent);
    if(*coordinate > world_extent) *coordinate = static_cast<float>(static_cast<calculation>(*coordinate) - world_extent);
  }
  flight.integer_position = vec3<int32_t>{truncate(flight.position.x),truncate(flight.position.y),truncate(flight.position.z)};
  // The original outer callback repeats this supplied contact after pose storage.
  if((flight.landed && state.ground.body.down.y < 0) || (ground.present && flight.position.y > ground.height)) {
    flight.position.y = ground.height;
    flight.collision = 2;
  }
  state.first_update = false;
  return {
    .horizon_motion{math::signed_word(static_cast<uint16_t>(truncate(static_cast<calculation>(flight.movement_per_tick))))},
    .gear_sounds{std::move(gear_sounds)},
  };
}

} // namespace tfx::flight
