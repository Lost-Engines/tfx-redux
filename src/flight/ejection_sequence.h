#pragma once

#include <optional>
#include "flight/ejection.h"

namespace tfx::flight {

enum class ejection_stage : uint8_t { cockpit, requested, pilot };
enum class ejection_event_kind { player_model, camera, music_cue, sample };
struct ejection_event {
  ejection_event_kind kind;
  unsigned int argument;
};
struct escape_result {
  bool end_flight{false};
  bool disable_refuelling{false};
  std::optional<vec3<int32_t>> fixed_camera{};
  std::vector<ejection_event> events{};
};
struct ejection_state {
  uint8_t escape_presses{0};
  ejection_stage stage{ejection_stage::cockpit};
  pilot_motion pilot{};
  vec3<int32_t> aircraft_position_q8{0,0,0};
  math::attitude aircraft_orientation{};
};
class ejection_sequence {
private:
  ejection_state state;

public:
  explicit ejection_sequence(ejection_state initial = {});
  ejection_state const &value() const;
  void reset();
  escape_result escape(vec3<int32_t> const &position, vec3<math::angle> const &angles, int32_t speed, bool confirmed = false);
  void request(); // SSD opcode 33, emitted at the authored seat-release frame.
  void collision(int16_t collision);
  pilot_update advance_pilot(pilot_context const &context);
  bool advance_aircraft(vec3f const &position, math::attitude const &orientation, int32_t speed, simulation::timer_ticks elapsed);
};

} // namespace tfx::flight
