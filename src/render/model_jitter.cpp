#include "model_jitter.h"

namespace tfx::render {

model_jitter::model_jitter(math::jitter_state const initial)
  : working{initial},
    saved{initial} {}
void model_jitter::begin(bool const animation_enabled) {
  if(animation_enabled) saved = working;
  working = saved;
}
math::jitter_state &model_jitter::state() {
  return working;
}

} // namespace tfx::render
