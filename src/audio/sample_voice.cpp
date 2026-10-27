#include "sample_voice.h"

#include <utility>

namespace tfx::audio {

sample_voice::sample_voice(sample_bank const &bank, uint32_t const output_rate, std::unique_ptr<qsound_effect> spatial) :
  bank{bank},
  scheduler{bank.scheduling()},
  output_rate{output_rate},
  spatial{std::move(spatial)} {
  if(!output_rate) throw std::invalid_argument{"PCM output rate must not be zero"};
}

void sample_voice::enable(bool const enabled) {
  std::lock_guard lock{mutex};
  scheduler.enable(enabled);
}

void sample_voice::advance(simulation::timer_ticks const elapsed) {
  std::lock_guard lock{mutex};
  scheduler.advance(elapsed);
}

void sample_voice::select_item() {
  source_period = sound_blaster_period(bank.entries()[queue[item]].rate);
  if(spatial) spatial->select(bank.entries()[queue[item]].qsound_parameters);
}

void sample_voice::submit(sample_priority const priority, std::span<unsigned int const> const sequence) {
  std::lock_guard lock{mutex};
  auto request{scheduler.submit(priority, sequence)};
  if(request.interrupted || request.started) {
    queue = std::move(request.sequence);
    item = offset = 0;
    phase = 0;
    latch_pending = request.started;
    tail_remaining = !request.started && spatial ? spatial->tail_samples() : 0;
    if(request.started) select_item();
  }
}

void sample_voice::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"PCM output needs complete stereo frames"};
  std::lock_guard lock{mutex};
  for(unsigned int frame{}; frame != stereo.size() / 2; ++frame) {
    // Keep the original shared timer running through idle and disabled streams.
    if(spatial) spatial->advance_frame();
    if(item == queue.size() && !tail_remaining) {
      if(!spatial) break;
      continue;
    }
    if(spatial) {
      if(latch_pending) {
        held = spatial->process(bank.pcm(queue[item])[offset]);
        latch_pending = false;
      }
      stereo[frame * 2] += held.left;
      stereo[frame * 2 + 1] += held.right;
    } else {
      auto const sample{bank.pcm(queue[item])[offset]};
      float const amplitude{(static_cast<int>(sample) - 128) / 128.0f};
      stereo[frame * 2] += amplitude;
      stereo[frame * 2 + 1] += amplitude;
    }
    phase += 1'000'000;
    while((item != queue.size() || tail_remaining) && phase >= static_cast<uint64_t>(source_period) * output_rate) {
      phase -= static_cast<uint64_t>(source_period) * output_rate;
      if(item != queue.size()) {
        if(++offset == bank.pcm(queue[item]).size()) {
          offset = 0;
          if(++item == queue.size()) {
            scheduler.complete();
            if(spatial) tail_remaining = spatial->tail_samples();
          } else {
            select_item();
          }
        }
      }
      if(spatial) {
        // Process every DAC transition, including samples skipped by a slower
        // host output rate. The filter never runs once per repeated host frame.
        if(item != queue.size()) held = spatial->process(bank.pcm(queue[item])[offset]);
        else if(tail_remaining) {
          held = spatial->process(128);
          if(!--tail_remaining) held = {};
        }
      }
    }
  }
}

sample_scheduler_state sample_voice::state() {
  std::lock_guard lock{mutex};
  return scheduler.value();
}

} // namespace tfx::audio
