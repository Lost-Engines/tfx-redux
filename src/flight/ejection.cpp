#include "ejection.h"
#include <bit>
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = double;
float constexpr normalised_word{3.0518509447574615e-05f};
float constexpr pilot_time_scale{.02f}, falling_drag{.005f}, parachute_drag{.02f};
double constexpr world_units_per_metre{.4101049868766404};
int32_t constexpr seat_burn_ticks{150}, canopy_open_ticks{25}, landing_wait_ticks{250};
int32_t wrap_add(int32_t const first, simulation::timer_ticks const second) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(first) + second);
}
calculation normalised(int32_t const value) {
  return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(value))) * normalised_word;
}
int32_t truncate(calculation const value) {
  // x87 FISTP with truncation returns integer-indefinite on an invalid conversion.
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void drift(pilot_motion &pilot, simulation::timer_ticks const elapsed) {
  for(unsigned int axis{0}; axis != 3; ++axis) pilot.position[axis] = static_cast<float>(static_cast<calculation>(pilot.velocity[axis]) * elapsed + pilot.position[axis]);
}
void damp(pilot_motion &pilot, simulation::timer_ticks const elapsed, float const amount) {
  for(auto &velocity : pilot.velocity) velocity = static_cast<float>(velocity - static_cast<calculation>(velocity) * amount * elapsed);
}
void gravity(pilot_motion &pilot, simulation::timer_ticks const elapsed) {
  float constexpr acceleration_metres{10};
  pilot.velocity.y = static_cast<float>(static_cast<calculation>(acceleration_metres) * world_units_per_metre * elapsed * pilot_time_scale * pilot_time_scale + pilot.velocity.y);
}

} // anonymous namespace

pilot_update advance_pilot(pilot_motion &pilot, pilot_context const &context) {
  pilot_update result;
  auto const elapsed{context.elapsed};
  switch(pilot.phase) {
  case pilot_phase::launch:
    pilot.orientation.rotate(math::rotation_axis::pitch,0xfa00);
    for(unsigned int axis{0}; axis != 3; ++axis) {
      pilot.position[axis] = static_cast<float>(normalised(pilot.orientation.down[axis]) * -1.0f + pilot.position[axis]);
      pilot.velocity[axis] = static_cast<float>(normalised(pilot.orientation.forward[axis]) * pilot.aircraft_speed * .00390625f);
    }
    drift(pilot,elapsed);
    pilot.elapsed = 0;
    pilot.parachute_open = false;
    pilot.auxiliary_counter = 0;
    pilot.phase = pilot_phase::seat_flight;
    break;
  case pilot_phase::seat_flight:
    pilot.elapsed = wrap_add(pilot.elapsed,elapsed);
    if(pilot.elapsed < seat_burn_ticks) {
      drift(pilot,elapsed);
      float const seat_acceleration{static_cast<float>(-24.0 * world_units_per_metre)};
      for(unsigned int axis{0}; axis != 3; ++axis) pilot.velocity[axis] = static_cast<float>(normalised(pilot.orientation.down[axis]) * seat_acceleration * elapsed * pilot_time_scale * pilot_time_scale + pilot.velocity[axis]);
      gravity(pilot,elapsed);
    } else {
      result.model_entries.push_back(7);
      pilot.phase = pilot_phase::falling;
    }
    break;
  case pilot_phase::falling:
    drift(pilot,elapsed);
    damp(pilot,elapsed,falling_drag);
    gravity(pilot,elapsed);
    if(static_cast<calculation>(5.0f) * world_units_per_metre * pilot_time_scale < pilot.velocity.y) {
      result.model_entries.push_back(3);
      pilot.elapsed = 0;
      pilot.phase = pilot_phase::opening;
    }
    break;
  case pilot_phase::opening:
    pilot.elapsed = wrap_add(pilot.elapsed,elapsed);
    drift(pilot,elapsed);
    damp(pilot,elapsed,falling_drag);
    gravity(pilot,elapsed);
    if(pilot.elapsed > canopy_open_ticks) {
      result.model_entries.push_back(4);
      pilot.parachute_open = true;
      pilot.phase = pilot_phase::parachute;
    }
    break;
  case pilot_phase::parachute:
    {
      vec3f direction;
      for(unsigned int axis{0}; axis != 3; ++axis) direction[axis] = static_cast<float>(normalised(pilot.orientation.down[axis]) + pilot.velocity[axis]);
      pilot.orientation.align_down(direction);
      drift(pilot,elapsed);
      damp(pilot,elapsed,parachute_drag);
      gravity(pilot,elapsed);
    }
    break;
  case pilot_phase::landed:
    pilot.elapsed = wrap_add(pilot.elapsed,elapsed);
    pilot.orientation.align_down({0,1,0});
    if(!pilot.ended && pilot.elapsed > landing_wait_ticks) {
      pilot.collision = 0;
      pilot.ended = true;
    }
    break;
  default: break; // The original dispatcher also leaves unused phases untouched.
  }
  // Native 0x8f198 retains its quit/collision ordering before ground placement.
  int16_t constexpr water_type{1};
  float constexpr water_surface_limit{-20};
  if((pilot.collision && !pilot.ended) || (pilot.position.y > water_surface_limit && (context.terrain_type == water_type || context.water_world))) {
    pilot.collision = pilot.parachute_open ? 0 : 1;
    pilot.ended = true;
  }
  if(static_cast<calculation>(context.ground_height) - 1 < pilot.position.y) {
    if(pilot.parachute_open) result.model_entries.push_back(5);
    else { result.model_entries.push_back(6);pilot.fatal = true; }
    pilot.position.y = static_cast<float>(context.ground_height - 1);
    pilot.elapsed = 0;
    pilot.phase = pilot_phase::landed;
  }
  for(unsigned int axis{0}; axis != 3; ++axis) pilot.integer_position[axis] = truncate(pilot.position[axis]);
  pilot.angles = pilot.orientation.angles();
  return result;
}

} // namespace tfx::flight
