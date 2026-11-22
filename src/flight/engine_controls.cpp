#include "engine_controls.h"

namespace tfx::flight {

engine_key_effects press_engine_key(engine_key_context const &context) {
  engine_key_effects result;
  if(context.inhibited || context.crashed || context.disabled) return result;
  if(context.condition == system_condition::active) {
    result.running = false;
    if(context.afterburner && context.other_condition == system_condition::normal) {
      result.ordered.push_back(engine_key_event::stop_afterburner);
    }
    result.ordered.push_back(engine_key_event::stop_sample);
    result.ordered.push_back(engine_key_event::stop_synth);
  } else if(context.condition == system_condition::normal) {
    result.running = true;
    if(context.side == engine_side::left) result.ordered.push_back(engine_key_event::start_samples);
    result.ordered.push_back(engine_key_event::start_synth);
    if(context.side == engine_side::right) result.ordered.push_back(engine_key_event::start_samples);
  }
  return result;
}

} // namespace tfx::flight
