#pragma once

#include <vector>
#include "math/attitude.h"
#include "simulation/timing.h"

namespace tfx::flight {

enum class pilot_phase : uint8_t {
  inactive,
  launch,
  seat_flight,
  falling,
  opening,
  parachute,
  landed = 10
};
struct pilot_motion {
  pilot_phase phase{pilot_phase::inactive};
  vec3f position{0, 0, 0};
  vec3f velocity{0, 0, 0};
  math::attitude orientation{};
  vec3<int32_t> integer_position{0, 0, 0};
  vec3<math::angle> angles{0, 0, 0};
  int32_t aircraft_speed{0};
  int32_t elapsed{0};
  bool parachute_open{false};
  int32_t auxiliary_counter{0};                                                // Native 0x13398 is cleared with the launch state.
  int16_t collision{0};
  bool ended{false};
  bool fatal{false};
};
struct pilot_context {
  simulation::timer_ticks elapsed;
  int16_t ground_height;
  int16_t terrain_type;
  bool water_world{false};
};
struct pilot_update {
  std::vector<unsigned int> model_entries{};
};
pilot_update advance_pilot(pilot_motion &pilot, pilot_context const &context);

} // namespace tfx::flight
