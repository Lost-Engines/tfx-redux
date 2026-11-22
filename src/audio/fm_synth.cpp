#include "fm_synth.h"
#include <algorithm>
#include <mutex>
#include <dbopl.h>

namespace tfx::audio {

struct fm_synth::implementation {
  DBOPL::Chip chip;
  unsigned int rate;
  uint64_t phase{0};
  float previous{0}, current{0};
  bool primed{false};
  float sample() {
    Bit32s value{0};
    chip.GenerateBlock2(1,&value);
    return static_cast<float>(std::clamp(value,-32768,32767)) / 32768.f;
  }
};
fm_synth::fm_synth(unsigned int const sample_rate) : state{std::make_unique<implementation>()} {
  if(!sample_rate) throw std::invalid_argument{"FM sample rate must be positive"};
  state->rate = sample_rate;
  static std::once_flag tables;
  std::call_once(tables,DBOPL::InitTables);
  state->chip.Setup(44100);
}
fm_synth::~fm_synth() = default;
void fm_synth::write(opl_write const command) {
  state->chip.WriteReg(command.address,command.value);
}
void fm_synth::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"FM output requires complete stereo frames"};
  // Keep the same reference DOSBox rate and causal sample conversion used by
  // the research auditions. Fractional phase survives every callback boundary.
  for(size_t index{0}; index != stereo.size(); index += 2) {
    if(state->rate == 44100) { stereo[index] = stereo[index + 1] = state->sample();continue; }
    if(!state->primed) { state->current = state->previous = state->sample();state->primed = true; }
    while(state->phase >= state->rate) {
      state->phase -= state->rate;state->previous = state->current;state->current = state->sample();
    }
    float const fraction{static_cast<float>(state->phase) / static_cast<float>(state->rate)};
    stereo[index] = stereo[index + 1] = state->previous + (state->current - state->previous) * fraction;
    state->phase += 44100;
  }
}

} // namespace tfx::audio
