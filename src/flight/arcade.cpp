#include "math/floating_environment.h"
#include "arcade.h"
#include "gear_controls.h"
#include "world/environment.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tfx::flight {
namespace {

using calculation = long double;
float constexpr sound_speed{340}, speed_metres_per_native_tick{122};
float constexpr normalised_word{3.0518509447574615e-05f};
float constexpr throttle_step{0.002f}, idle_throttle{0.6f};
float constexpr mouse_angle_step{-22}, control_response{1.0f / 12};
float constexpr speed_response{0.003f}, simple_speed_response{.0015f}, brake_response{0.01f}, arrested_response{0.02f};
float constexpr fixed_speed_scale{256}, altitude_scale{feet_per_world_unit};
double constexpr feet_to_metres{0.3047999902464003}, metres_to_km{0.001};
double constexpr world_units_per_kilometre{0.0024383999407291412f};
float constexpr fuel_rate{0.008f};

std::array<basic_aircraft_parameters,3> constexpr performance_table{{
  {
    .low_stall_mach{0.15f},
    .low_altitude{0},
    .middle_stall_mach{0.35f},
    .middle_altitude{17},
    .high_stall_mach{2},
    .ceiling_km{18.5f},
    .high_maximum_mach{2},
    .maximum_speed_altitude{11},
    .low_maximum_mach{1.3f},
    .maximum_speed_base_altitude{0},
    .gear_height{2.675f},
    .stall_metres_per_second{64.375},
    .fuel_capacity{4000},
    .dry_thrust_fraction{0.65},
    .afterburner_stages{4},
    .maximum_g{9},
    .minimum_g{-3},
    .low_speed_reference{61.8},
    .gear_airborne_speed_limit{200},
    .gear_ground_speed_limit{87.5},
    .gear_touchdown_descent_limit{3.1},
  },
  {
    .low_stall_mach{0.15f},
    .low_altitude{0},
    .middle_stall_mach{0.7f},
    .middle_altitude{15.5f},
    .high_stall_mach{0.9f},
    .ceiling_km{16.5f},
    .high_maximum_mach{0.9f},
    .maximum_speed_altitude{11},
    .low_maximum_mach{0.9f},
    .maximum_speed_base_altitude{0},
    .gear_height{2.225f},
    .stall_metres_per_second{92.7},
    .fuel_capacity{7938},
    .dry_thrust_fraction{1},
    .afterburner_stages{0},
    .maximum_g{6},
    .minimum_g{-1},
    .low_speed_reference{79.825},
    .gear_airborne_speed_limit{205},
    .gear_ground_speed_limit{120},
    .gear_touchdown_descent_limit{5},
  },
  {
    .low_stall_mach{0.15f},
    .low_altitude{0},
    .middle_stall_mach{0.5f},
    .middle_altitude{19},
    .high_stall_mach{2.3f},
    .ceiling_km{20},
    .high_maximum_mach{2.3f},
    .maximum_speed_altitude{12},
    .low_maximum_mach{1.2f},
    .maximum_speed_base_altitude{0},
    .gear_height{2.8f},
    .stall_metres_per_second{67.98},
    .fuel_capacity{9979},
    .dry_thrust_fraction{0.85},
    .afterburner_stages{4},
    .maximum_g{9},
    .minimum_g{-3},
    .low_speed_reference{77.25},
    .gear_airborne_speed_limit{160},
    .gear_ground_speed_limit{110},
    .gear_touchdown_descent_limit{3.05},
  },
}};
basic_aircraft_parameters const &parameters(player_aircraft const aircraft) {
  return performance_table.at(std::to_underlying(aircraft) - 1);
}
calculation interpolate(float const low_value, float const low_altitude, float const high_value, float const high_altitude, float const altitude) {
  // native 0x8c50c divides the supplied altitude directly by the interval, without subtracting its lower endpoint
  return (static_cast<calculation>(high_value) - low_value) * (static_cast<calculation>(altitude) / (static_cast<calculation>(high_altitude) - low_altitude)) + low_value;
}
int32_t integer(calculation const value) {
  if(value >= 2147483648.0L || value < -2147483648.0L || !std::isfinite(value)) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void rotate(arcade_state &state, math::rotation_axis const axis, float const amount) {
  unsigned int const index{static_cast<unsigned int>(axis)};
  auto &remainder{state.rotation_remainder[index]};
  calculation const accumulated{static_cast<calculation>(remainder) + amount};
  remainder = static_cast<float>(accumulated);
  int32_t const units{integer(accumulated)};
  auto const rotation{static_cast<uint16_t>(units)};
  remainder = static_cast<float>(static_cast<calculation>(remainder) - math::signed_word(rotation));
  state.orientation.rotate(axis,rotation);
}
void recover(arcade_state &state, uint16_t const elapsed) {
  if(state.landed) return;
  auto const angles{state.orientation.angles()};
  int32_t const pitch{math::signed_word(angles.x)}, heading{math::signed_word(angles.y)};
  state.orientation = state.orientation.transposed();
  rotate(state,math::rotation_axis::heading,static_cast<float>(heading));
  int32_t rate{-1048576};
  if(pitch > 0) {
    rate = -math::cosine_q15(pitch * 2) * 32;
  }
  int32_t const delta{std::bit_cast<int32_t>(static_cast<uint32_t>(rate) * elapsed) >> 15};
  rotate(state,math::rotation_axis::pitch,static_cast<float>(delta));
  rotate(state,math::rotation_axis::heading,static_cast<float>(-heading));
  state.orientation = state.orientation.transposed();
  state.recovering = true;
}
void throttle(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, uint16_t const ticks, bool const disabled, bool const arcade, bool const gear_drag) {
  auto const &model{parameters(state.aircraft)};
  calculation const movement{static_cast<calculation>(ticks) * throttle_step};
  if(state.left_engine || state.right_engine) {
    if((controls.mouse_buttons & 1) || controls.accelerate) state.throttle = static_cast<float>(static_cast<calculation>(state.throttle) + movement);
    else if(static_cast<calculation>(state.throttle) < 0.6) {
      calculation const value{static_cast<calculation>(state.throttle) + movement};
      state.throttle = value > 0.6 ? idle_throttle : static_cast<float>(value);
    }
    if(((controls.mouse_buttons & 2) || controls.decelerate) && static_cast<calculation>(state.throttle) > 0.6) {
      calculation const value{static_cast<calculation>(state.throttle) - movement};
      state.throttle = value < 0.6 ? idle_throttle : static_cast<float>(value);
    }
    if(state.afterburner) state.throttle = 1;
  } else state.throttle = 0;
  state.throttle = std::clamp(state.throttle,0.0f,1.0f);
  if(state.throttle == 1) {
    if((controls.mouse_buttons & 1) && !(state.previous_mouse_buttons & 1) && state.afterburner < model.afterburner_stages && (state.left_engine || state.right_engine) && state.fuel > 0) ++state.afterburner;
    if((controls.mouse_buttons & 2) && !(state.previous_mouse_buttons & 2) && state.afterburner) --state.afterburner;
  }
  state.previous_mouse_buttons = controls.mouse_buttons;
  calculation const altitude{-static_cast<calculation>(state.position.y) * altitude_scale * feet_to_metres * metres_to_km};
  state.altitude_km = static_cast<float>(altitude);
  if(altitude > model.ceiling_km) {
    state.minimum_speed = model.high_stall_mach;
    state.maximum_speed = state.minimum_speed;
    state.altitude_km = model.ceiling_km;
    state.position.y = static_cast<float>(-static_cast<calculation>(state.altitude_km) * 1000.0f * 3.28084 * 0.125);
  } else {
    state.minimum_speed = static_cast<float>(state.altitude_km < model.middle_altitude ? interpolate(model.low_stall_mach,model.low_altitude,model.middle_stall_mach,model.middle_altitude,state.altitude_km) : interpolate(model.middle_stall_mach,model.middle_altitude,model.high_stall_mach,model.ceiling_km,state.altitude_km));
    state.maximum_speed = static_cast<float>(state.altitude_km < model.maximum_speed_altitude ? interpolate(model.low_maximum_mach,model.maximum_speed_base_altitude,model.high_maximum_mach,model.maximum_speed_altitude,state.altitude_km) : interpolate(model.high_maximum_mach,model.maximum_speed_altitude,model.high_stall_mach,model.ceiling_km,state.altitude_km));
  }
  if(state.landed) state.minimum_speed = 0;
  state.minimum_speed = std::min(state.minimum_speed,state.maximum_speed);
  if(static_cast<calculation>(state.throttle) > 0.6) {
    float const span{static_cast<float>((static_cast<calculation>(state.maximum_speed) - state.minimum_speed) * model.dry_thrust_fraction)};
    state.target_speed = static_cast<float>((static_cast<calculation>(state.throttle) - 0.6) * span * 2.5 + state.minimum_speed);
    if(model.afterburner_stages) state.target_speed = static_cast<float>(static_cast<calculation>(state.target_speed) + (static_cast<calculation>(state.maximum_speed) - span) * state.afterburner / model.afterburner_stages);
  } else state.target_speed = 0;
  if(disabled) state.target_speed = 0.5f; // Native 0x8ca91 fixes the requested Mach after loss/ejection.
  if(gear_drag) state.target_speed = static_cast<float>(static_cast<calculation>(state.target_speed) * 0.95f);
  if(state.air_brake) state.target_speed = static_cast<float>(static_cast<calculation>(state.target_speed) * 0.8f);
  // Native 8CADA reads the indicators, while throttle input and fuel use the
  // engine factors. The outer frame normalizes these indicators afterwards.
  bool const left_off{engine_conditions.x == system_condition::normal};
  bool const right_off{engine_conditions.y == system_condition::normal};
  if(left_off && right_off) { state.target_speed = 0; state.arrested = false; }
  else if(left_off || right_off) state.target_speed = static_cast<float>(static_cast<calculation>(state.target_speed) * 0.5f);
  float response{arcade ? speed_response : simple_speed_response};
  if((state.landed && state.wheel_brake) || state.brake_chute) { state.target_speed = 0; response = brake_response; }
  if(state.arrested) { state.target_speed = 0; response = arrested_response; }
  if(!state.landed && !(state.speed_mach > state.minimum_speed && state.minimum_speed != state.maximum_speed && state.altitude_km <= model.ceiling_km)) {
    state.speed_mach = state.minimum_speed;
    if(state.target_speed == 0) state.target_speed = static_cast<float>(static_cast<calculation>(state.minimum_speed) - 0.1f);
    recover(state,ticks);
  }
  calculation const speed{(static_cast<calculation>(state.target_speed) - state.speed_mach) * response * (static_cast<int32_t>(state.brake_chute) + 1) * ticks + state.speed_mach};
  state.speed_mach = static_cast<float>(speed);
  state.unrounded_speed_mach = static_cast<double>(speed);
  if(state.frozen && state.crashing) state.speed_mach = 0;
  state.speed_metres_per_second = static_cast<float>(static_cast<calculation>(state.speed_mach) * sound_speed);
}
void fuel(arcade_state &state, uint16_t const ticks, bool const arcade = true) {
  state.fuel_flow = static_cast<double>(static_cast<calculation>(state.fixed_speed) * 0.515f * fuel_rate);
  if(state.left_engine != state.right_engine) state.fuel_flow = static_cast<double>(static_cast<calculation>(state.fuel_flow) * 0.5);
  else if(!state.left_engine) state.fuel_flow = 0;
  if(arcade) state.fuel_flow = static_cast<double>(static_cast<calculation>(state.fuel_flow) * 0.5);
  state.fuel = static_cast<double>(static_cast<calculation>(state.fuel) - static_cast<calculation>(ticks) * state.fuel_flow * 0.02);
  state.fuel = std::max(state.fuel,0.0);
  auto const whole_fuel{math::signed_word(static_cast<uint16_t>(integer(state.fuel)))};
  state.fuel_readout = math::signed_word(static_cast<uint16_t>(integer(static_cast<calculation>(whole_fuel) / (static_cast<calculation>(state.fuel_capacity) + state.extra_fuel_capacity) * 9999.0)));
  if(state.fuel == 0) {
    state.afterburner = 0;
    state.left_engine = false;
    state.right_engine = false;
  }
}
void filter_control(float &target, float &current, int16_t const delta, uint16_t const ticks, float &change) {
  target = static_cast<float>(static_cast<calculation>(delta) * mouse_angle_step + target);
  float const difference{static_cast<float>(static_cast<calculation>(target) - current)};
  change = static_cast<float>(static_cast<calculation>(ticks) * difference * control_response);
  if((static_cast<calculation>(difference) - change) * difference < 0) change = difference;
  current = static_cast<float>(static_cast<calculation>(current) + change);
}

} // anonymous namespace

basic_aircraft_parameters const &basic_parameters(player_aircraft const aircraft) { return parameters(aircraft); }
void update_basic_speed(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, bool const arcade, bool const gear_drag) {
  math::floating_environment const environment;
  throttle(state,controls,engine_conditions,elapsed,false,arcade,gear_drag);
}
void consume_basic_fuel(arcade_state &state, simulation::timer_ticks const elapsed, bool const arcade) {
  math::floating_environment const environment;
  fuel(state,elapsed,arcade);
}


void initialise_arcade(arcade_state &state, vec3<int32_t> const &position, vec3<math::angle> const &angles, float const speed, bool const landed, ground_contact const ground) {
  math::floating_environment const environment;
  /// Native mission start centres X/Z on their cells; ground placement uses the aircraft's own gear height
  if constexpr(std::numeric_limits<calculation>::digits != 64) throw std::runtime_error{"native flight currently requires an x87-precision arithmetic backend"};
  auto const &model{parameters(state.aircraft)};
  state.position = vec3f{vec3<int32_t>{(position.x / world::cell_units) * world::cell_units + world::cell_units / 2,position.y,(position.z / world::cell_units) * world::cell_units + world::cell_units / 2}};
  state.orientation = {};
  state.orientation.rotate(math::rotation_axis::pitch,angles.x);
  state.orientation.rotate(math::rotation_axis::heading,angles.y);
  state.orientation.rotate(math::rotation_axis::roll,angles.z);
  state.rotation_remainder = vec3f{0,0,0};
  state.control_target = vec2f{0,0};
  state.filtered_control = vec2f{0,0};
  state.gear_down = landed;
  state.landed = landed;
  state.low_speed_ground_contact = landed;
  state.left_engine = !landed;
  state.right_engine = !landed;
  state.throttle = landed ? 0 : .85f;
  state.speed_mach = landed ? 0 : speed;
  state.fuel_capacity = model.fuel_capacity;
  state.extra_fuel_capacity = state.aircraft == player_aircraft::eurofighter ? model.fuel_capacity : 0;
  state.fuel = model.fuel_capacity;
  if(landed) {
    float constexpr runway_start_setback{120};
    state.position.y = static_cast<float>(static_cast<calculation>(ground.height) - model.gear_height);
    if(ground.height >= -10) {
      // The first native player callback places a runway start 120 world units behind the supplied cell centre.
      state.position.x = static_cast<float>(static_cast<calculation>(state.position.x) - static_cast<calculation>(state.orientation.forward.x) * normalised_word * runway_start_setback);
      state.position.z = static_cast<float>(static_cast<calculation>(state.position.z) - static_cast<calculation>(state.orientation.forward.z) * normalised_word * runway_start_setback);
    }
  }
  state.integer_position = vec3<int32_t>{state.position};
  state.angles = state.orientation.angles();
}

void update_basic_disabled(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, ground_contact const ground, bool const arcade, bool const gear_drag) {
  math::floating_environment const environment;
  // Native 0x8e5a8, selected by the outer player callback after loss/ejection.
  if(!state.collision) {
    recover(state,elapsed);
    if(state.position.y < 0) {
      throttle(state,controls,engine_conditions,elapsed,true,arcade,gear_drag);
      calculation const movement{static_cast<calculation>(state.speed_mach) * sound_speed * (1.0f / speed_metres_per_native_tick)};
      state.movement_per_tick = static_cast<float>(movement);
      state.fixed_speed = integer(movement * fixed_speed_scale);
      rotate(state,math::rotation_axis::roll,-static_cast<float>(elapsed * 128u));
      if(state.normalisation_counter & 31) state.orientation.normalise();
      ++state.normalisation_counter;
      auto const &forward{state.orientation.forward};
      auto const moved{[&](float const value, int32_t const direction) { return static_cast<float>(static_cast<calculation>(direction) * normalised_word * elapsed * state.movement_per_tick + value); }};
      state.position = vec3f{moved(state.position.x,forward.x),moved(state.position.y,forward.y),moved(state.position.z,forward.z)};
      state.position.y = static_cast<float>(static_cast<calculation>(elapsed) * state.movement_per_tick * 0.125f + state.position.y);
    }
    if(ground.present && state.position.y >= ground.height) { state.position.y = ground.height; state.collision = 2; }
    else if(state.position.y >= 0) { state.position.y = 0; state.collision = 1; }
  } else state.orientation = {};
  state.integer_position = vec3<int32_t>{integer(state.position.x),integer(state.position.y),integer(state.position.z)};
  state.angles = state.orientation.angles();
  // The outer 0x8f56c callback repeats this supplied contact check after motion.
  if((state.landed && state.orientation.down.y < 0) || (ground.present && state.position.y > ground.height)) { state.position.y = ground.height; state.collision = 2; }
}

void advance_disabled(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, ground_contact const ground) {
  math::floating_environment const environment;
  update_basic_disabled(state,controls,engine_conditions,elapsed,ground,true,state.gear_down);
}

arcade_update advance_arcade(arcade_state &state, flight_controls const &controls, vec2<system_condition> const &engine_conditions, simulation::timer_ticks const elapsed, ground_contact const ground, bool const time_warp) {
  math::floating_environment const environment;
  /// Native Arcade 0x8dc50: throttle/fuel, gear contact, filtered controls, coordinated bank turn, then movement
  auto const &model{parameters(state.aircraft)};
  if(state.aircraft == player_aircraft::eurofighter && state.gear_down && state.fixed_speed > eurofighter_maximum_gear_speed) state.gear_down = false;
  if(state.landed && state.fixed_speed < 5 && state.brake_chute) { state.brake_chute = false; state.brake_chute_cut_away = true; }
  state.recovering = false;
  throttle(state,controls,engine_conditions,elapsed,false,true,state.gear_down);
  calculation const movement{static_cast<calculation>(state.speed_mach) * sound_speed * (1.0f / speed_metres_per_native_tick)};
  state.movement_per_tick = static_cast<float>(movement);
  state.fixed_speed = integer(movement * fixed_speed_scale);
  fuel(state,elapsed);
  if(state.speed_metres_per_second <= model.stall_metres_per_second) state.position.y = static_cast<float>(static_cast<calculation>(state.position.y) + static_cast<calculation>(elapsed) * 0.1f);
  float const gear_height{static_cast<float>(static_cast<calculation>(ground.height) - model.gear_height)};
  if(ground.present && state.position.y >= gear_height) {
    if(state.gear_down && state.orientation.down.y >= 0) {
      state.position.y = gear_height;
      auto const angles{state.orientation.angles()};
      int32_t const pitch{math::signed_word(angles.x) / 2}, roll{math::signed_word(angles.z) / 2};
      rotate(state,math::rotation_axis::pitch,static_cast<float>(-pitch));
      rotate(state,math::rotation_axis::roll,static_cast<float>(-roll));
      state.landed = true;
      state.low_speed_ground_contact = state.fixed_speed < 60;
    } else {
      state.position.y = ground.height;
      state.collision = 2;
    }
  } else { state.landed = false; state.low_speed_ground_contact = false; }
  float pitch_change{0}, roll_change{0};
  filter_control(state.control_target.x,state.filtered_control.x,controls.pitch_delta,elapsed,pitch_change);
  if(!state.landed || state.speed_metres_per_second > model.stall_metres_per_second) rotate(state,math::rotation_axis::pitch,pitch_change);
  filter_control(state.control_target.y,state.filtered_control.y,controls.roll_delta,elapsed,roll_change);
  if(!state.landed || state.speed_metres_per_second > model.stall_metres_per_second) rotate(state,math::rotation_axis::roll,roll_change);
  else rotate(state,math::rotation_axis::heading,static_cast<float>(-static_cast<calculation>(roll_change) * state.fixed_speed * 0.005f));
  auto turn{state.orientation.transposed()};
  uint16_t const turn_ticks{static_cast<uint16_t>(time_warp ? elapsed / 5 : elapsed)};
  auto const bank{state.orientation.angles().z};
  int const bank_sine{std::clamp(math::sine_q15(bank),-16384,16384)};
  auto const half_pitch{static_cast<uint16_t>(state.orientation.forward.y / 2)};
  int32_t const turn_rate{(math::cosine_q15(half_pitch) * bank_sine) >> 15};
  turn.rotate(math::rotation_axis::heading,static_cast<uint16_t>(math::signed_word(static_cast<uint16_t>(turn_rate)) * turn_ticks / 80));
  state.orientation = turn.transposed();
  if(state.normalisation_counter & 31) state.orientation.normalise();
  ++state.normalisation_counter;
  vec3f const movement_origin{state.position};
  if(!state.frozen) {
    auto const &forward{state.orientation.forward};
    auto moved{[&](float const value, int32_t const direction){ return static_cast<float>(static_cast<calculation>(direction) * normalised_word * elapsed * state.fixed_speed * (1.0f / fixed_speed_scale) + value); }};
    state.position = vec3f{moved(state.position.x,forward.x),moved(state.position.y,forward.y),moved(state.position.z,forward.z)};
  }
  state.angles = state.orientation.angles();
  state.integer_position = vec3<int32_t>{integer(state.position.x),integer(state.position.y),integer(state.position.z)};
  if(state.position.y > 0) {
    state.position.y = state.crashing ? 0 : -5;
    state.collision = state.crashing ? 1 : 0;
  }
  float const ceiling{-static_cast<float>(integer(static_cast<calculation>(model.ceiling_km) / world_units_per_kilometre))};
  state.position.y = std::max(state.position.y,ceiling);
  // The outer callback repeats its contact check after motion; integer pose stores precede this clamp.
  if((state.landed && state.orientation.down.y < 0) || (ground.present && state.position.y > ground.height)) {
    state.position.y = ground.height;
    state.collision = 2;
  }
  return {
    .movement_origin{vec3f{movement_origin}},
    .horizon_motion{math::signed_word(static_cast<uint16_t>(state.fixed_speed / 256))},
  };
}

} // namespace tfx::flight
