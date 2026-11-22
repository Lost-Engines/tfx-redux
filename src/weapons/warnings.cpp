#include "warnings.h"
#include <algorithm>

namespace tfx::weapons {

void begin_warning_step(warning_state &state, simulation::timer_ticks const elapsed) {
  // Native 0xa31cc resets the count on every projectile substep, including a zero-tick update.
  state.cooldown = std::max<int16_t>(0, math::signed_word(static_cast<uint16_t>(state.cooldown - elapsed)));
  state.incoming = 0;
}

std::optional<projectile_warning> observe_player_threat(warning_state &state, int32_t const distance, bool const targeted, bool const inhibited, math::random_state &random) {
  int32_t constexpr count_distance{8000};
  int32_t constexpr spoken_distance{2000};                                     // Integer world coordinates, before Q8 actor scaling.
  int16_t constexpr warning_interval{500};                                     // Shared game timer ticks, not rendered frames.
  if(!targeted) return std::nullopt;
  if(distance < count_distance) ++state.incoming;
  if(distance >= spoken_distance || state.cooldown || inhibited) return std::nullopt;
  state.cooldown = warning_interval;
  // The original consumes shared game randomness even when audio output is disabled.
  unsigned int const choice{random.next() & 15u};
  return projectile_warning{
    .sample{choice <= 5 ? 17u : choice <= 10 ? 28u : 23u},
    .music{true},
  };
}

std::optional<projectile_warning> finish_warning_step(warning_state &state) {
  // Native 0xa34c0 requests a separate warning for two or more missiles, then clears its missile count.
  if(math::signed_word(state.incoming) <= 1) return std::nullopt;
  unsigned int const sample{state.incoming == 2 ? 38u : 39u};
  state.incoming = 0;
  return projectile_warning{
    .sample{sample},
  };
}

} // namespace tfx::weapons
