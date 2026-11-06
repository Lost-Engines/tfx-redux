#include "g_effects.h"
#include <algorithm>

namespace tfx::render {

void advance_g_effects(g_effects_state &state, float const load, configuration::g_effects const realism, simulation::timer_ticks const elapsed) {
  using calculation = double;
  float const negative_limit{realism == configuration::g_effects::reduced ? 5.f : 4.f};
  float const positive_limit{realism == configuration::g_effects::reduced ? 10.f : 9.f};
  float constexpr excess_gain{.2f}, native_hz{54};
  float constexpr redout_recovery_per_tick{.25f}, blackout_recovery_per_tick{.5f};
  float constexpr minimum_strain{-50}, maximum_strain{50};
  float constexpr redout_clear_level{50}, blackout_clear_level{49};
  float increment{0};
  if(load < -negative_limit) increment = static_cast<float>((static_cast<calculation>(negative_limit) + load) * excess_gain * native_hz);
  else if(load > positive_limit) increment = static_cast<float>((static_cast<calculation>(load) - positive_limit) * excess_gain * native_hz);
  if(state.strain < 0) {
    state.strain = static_cast<float>(static_cast<calculation>(elapsed) * redout_recovery_per_tick + state.strain);
    if(state.strain > 0) state.strain = 0;
  } else if(state.strain > 0) {
    state.strain = static_cast<float>(static_cast<calculation>(state.strain) - static_cast<calculation>(elapsed) * blackout_recovery_per_tick);
    if(state.strain < 0) state.strain = 0;
  }
  calculation const accumulated{static_cast<calculation>(state.strain) + increment};
  state.strain = static_cast<float>(accumulated);
  // FCOMP/JAE also sends unordered input to the lower clamp in the original.
  if(!(accumulated >= minimum_strain)) state.strain = minimum_strain;
  if(state.strain > maximum_strain) state.strain = maximum_strain;
  if(state.strain < 0) {
    state.redout_level = std::max(int16_t{0},static_cast<int16_t>(static_cast<int32_t>(static_cast<calculation>(redout_clear_level) + state.strain)));
    state.blackout_level = 0;
    state.mode = blackout_mode::redout;
  } else if(state.strain > 0) {
    state.blackout_level = std::max(int16_t{0},static_cast<int16_t>(static_cast<int32_t>(static_cast<calculation>(blackout_clear_level) - state.strain)));
    state.redout_level = 0;
    state.mode = blackout_mode::blackout;
  } else state.mode = blackout_mode::none;
}

} // namespace tfx::render
