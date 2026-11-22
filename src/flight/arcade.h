#pragma once

#include "math/attitude.h"
#include "flight/basic_parameters.h"
#include "flight/damage.h"
#include "flight/stores.h"
#include "simulation/timing.h"

namespace tfx::flight {

unsigned int constexpr feet_per_world_unit{8};

struct flight_controls {
  int16_t pitch_delta{0};
  int16_t roll_delta{0};
  bool accelerate{false};
  bool decelerate{false};
  uint8_t mouse_buttons{0};
};
struct ground_contact {
  bool present{false};
  int16_t height{0};
};
struct arcade_state {
  player_aircraft aircraft{player_aircraft::eurofighter};
  vec3f position{0,0,0};
  vec3<int32_t> integer_position{0,0,0};
  math::attitude orientation{};
  vec3f rotation_remainder{0,0,0};
  vec2f control_target{0,0};
  vec2f filtered_control{0,0};
  uint16_t normalisation_counter{0};
  vec3<math::angle> angles{0,0,0};
  float speed_mach{1};
  float throttle{0};
  float minimum_speed{0};
  float maximum_speed{0};
  float target_speed{0};
  float speed_metres_per_second{0};
  float altitude_km{0};
  float movement_per_tick{0};
  double unrounded_speed_mach{0};
  int32_t fixed_speed{0};
  bool landed{false};
  bool low_speed_ground_contact{false};
  bool gear_down{false};
  bool air_brake{false};
  bool wheel_brake{false};
  bool crashing{true};
  bool frozen{false};
  bool arrested{false};
  bool arrestor_hook{false};
  bool recovering{false};
  int16_t collision{0};
  bool brake_chute{false};
  bool brake_chute_cut_away{false};
  bool left_engine{true};
  bool right_engine{true};
  uint8_t afterburner{0};
  uint8_t previous_mouse_buttons{0};
  double fuel{0};
  double fuel_capacity{0};
  double extra_fuel_capacity{0};
  double fuel_flow{0};
  int16_t fuel_readout{0};
};

void initialise_arcade(arcade_state &state, vec3<int32_t> const &position, vec3<math::angle> const &angles, float speed, bool landed, ground_contact ground = {});
// Shared native 0x8c6e8/0x8cc80 stages; Simple consumes twice Arcade's fuel.
// Engine factors and indicator bytes are distinct native inputs. A newly
// failed engine has zero power before its indicator becomes normal/off.
void update_basic_speed(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, bool arcade, bool gear_drag);
void consume_basic_fuel(arcade_state &state, simulation::timer_ticks elapsed, bool arcade);
void update_basic_disabled(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, ground_contact ground, bool arcade, bool gear_drag);
void advance_disabled(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, ground_contact ground = {});
struct arcade_update {
  vec3f movement_origin;
  int16_t horizon_motion{0};
};
// Native Arcade overwrites the outer movement snapshot after ground/gear
// adjustments, immediately before translation. The frame tail measures from it.
arcade_update advance_arcade(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks elapsed, ground_contact ground = {}, bool time_warp = false);

} // namespace tfx::flight
