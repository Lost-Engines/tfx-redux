#include "ejection_sequence.h"
#include <cmath>
#include <limits>
#include <utility>

namespace tfx::flight {
namespace {
int32_t truncate(long double const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0L) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
} // anonymous namespace

ejection_sequence::ejection_sequence(ejection_state initial) : state{std::move(initial)} {}
ejection_state const &ejection_sequence::value() const { return state; }
void ejection_sequence::reset() { state = {}; }
void ejection_sequence::request() { state.stage = ejection_stage::requested; }
escape_result ejection_sequence::escape(vec3<int32_t> const &position, vec3<math::angle> const &angles, int32_t const speed, bool const confirmed) {
  escape_result result;
  if(state.stage != ejection_stage::cockpit) { result.end_flight = true;return result; }
  if(confirmed) state.escape_presses = 1;
  ++state.escape_presses;
  if(state.escape_presses != 2) return result;
  state.escape_presses = 0;
  result.disable_refuelling = true;
  // Native 0x84e60 requests entry 11, then 0x83ee4 requests it a second time.
  unsigned int constexpr seat_entry{11}, ejection_camera{21}, ejection_music_cue{6}, seat_sample{20};
  result.events = {
    {
      .kind{ejection_event_kind::player_model},
      .argument{seat_entry},
    },
    {
      .kind{ejection_event_kind::player_model},
      .argument{seat_entry},
    },
    {
      .kind{ejection_event_kind::camera},
      .argument{ejection_camera},
    },
    {
      .kind{ejection_event_kind::music_cue},
      .argument{ejection_music_cue},
    },
    {
      .kind{ejection_event_kind::sample},
      .argument{seat_sample},
    },
  };
  auto camera{vec3<int32_t>{position}};
  int constexpr pitch_bias{150}, heading_bias{200};
  vec3<math::angle> const departure{
    static_cast<math::angle>(angles.x - pitch_bias),
    static_cast<math::angle>(angles.y - heading_bias),
    angles.z,
  };
  math::advance_position(camera,departure,math::signed_word(static_cast<uint16_t>(speed >> 1)),-16384,0);
  result.fixed_camera.emplace(std::move(camera));
  return result;
}
void ejection_sequence::collision(int16_t const collision) { state.pilot.collision = collision; }
pilot_update ejection_sequence::advance_pilot(pilot_context const &context) {
  return flight::advance_pilot(state.pilot,context);
}
bool ejection_sequence::advance_aircraft(vec3f const &position, math::attitude const &orientation, int32_t const speed, simulation::timer_ticks const elapsed) {
  bool activated{false};
  if(state.stage == ejection_stage::requested) {
    float constexpr position_scale{256};
    for(unsigned int axis{0}; axis != 3; ++axis) state.aircraft_position_q8[axis] = truncate(static_cast<long double>(position[axis]) * position_scale);
    state.aircraft_orientation = orientation;
    state.stage = ejection_stage::pilot;
    state.pilot.phase = pilot_phase::launch;
    state.pilot.position = vec3f{position};
    state.pilot.orientation = orientation;
    state.pilot.aircraft_speed = speed;
    activated = true;
  }
  if(state.stage == ejection_stage::pilot && state.pilot.phase != pilot_phase::launch) {
    float constexpr normalised_word{3.0518509447574615e-05f};
    for(unsigned int axis{0}; axis != 3; ++axis) state.aircraft_position_q8[axis] = truncate(static_cast<long double>(math::signed_word(static_cast<uint16_t>(state.aircraft_orientation.forward[axis]))) * normalised_word * elapsed * speed + state.aircraft_position_q8[axis]);
  }
  return activated;
}

} // namespace tfx::flight
