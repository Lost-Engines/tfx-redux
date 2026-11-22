#pragma once

#include <optional>
#include "actors/program.h"
#include "simulation/timing.h"

namespace tfx::actors {

int constexpr air_control_horizon{55};
unsigned int constexpr envelope_classes{6}, envelope_altitudes{10}, envelope_columns{19};
unsigned int constexpr envelope_bytes{envelope_classes * envelope_altitudes * envelope_columns};

enum class air_flag : uint16_t {
  airborne = 1,
  joining_formation = 2,
  waypoint_steering = 4,
  hostile = 8,
  awaiting_takeoff = 16,
  afterburner = 32,
  new_waypoint = 64,
  crashed = 128,
  retired = 256,
  afterburner_display = 512,
};
enum class air_visual_event : uint8_t {
  afterburner_on = 9,
  afterburner_off
};
struct air_performance {
  int32_t acceleration;
  int32_t deceleration;
  uint16_t envelope;
  uint16_t turn_dividend;
  uint16_t minimum_speed;
  uint16_t maximum_speed;
  uint16_t minimum_altitude;
  uint16_t maximum_altitude;
  uint16_t afterburner_speed;
  int16_t maximum_roll_rate;
};
struct air_state {
  initial_actor pose{};
  vec3<int16_t> angular_rate{0, 0, 0};
  int32_t acceleration{0};
  int16_t input_ticks{0};
};
struct air_control_context {
  simulation::flight_model model{simulation::flight_model::arcade};
  std::span<uint8_t const> turn_envelope{};
};

air_performance decode_air_performance(profile const &source);
void reset_air_controls(air_state &state);
uint16_t maximum_turn_rate(air_state const &state, air_performance const &performance, air_control_context const &context = {});
uint16_t cruise_speed(air_state const &state, air_performance const &performance, std::span<uint8_t const> envelope);
void command_speed(air_state &state, air_performance const &performance, int32_t requested);
void command_pitch(air_state &state, air_performance const &performance, math::angle pitch, air_control_context const &context = {});
bool command_turn(air_state &state, air_performance const &performance, int16_t error, air_control_context const &context = {});
int16_t altitude_pitch(air_state const &state, air_performance const &performance, int32_t requested);
void shorten_air_step(air_state &state, int32_t distance);
std::optional<air_visual_event> advance_air_motion(air_state &state);

} // namespace tfx::actors
