#pragma once

#include "configuration/settings.h"
#include "render/palette_lighting.h"

namespace tfx::render {

struct g_effects_state {
  float strain{0};                                                             // Native signed visual strain, clamped to -50..50.
  blackout_mode mode{blackout_mode::none};
  int16_t blackout_level{0};
  int16_t redout_level{0};
};

// Native 0x73654. The outer caller skips/resets this controller when G effects
// are disabled; the helper itself uses realistic limits for every non-reduced
// code, including zero. Colour levels intentionally remain retained at zero strain.
void advance_g_effects(g_effects_state &state, float load, configuration::g_effects realism, simulation::timer_ticks elapsed);

} // namespace tfx::render
