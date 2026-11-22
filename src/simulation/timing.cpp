#include "timing.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include "math/native.h"

namespace tfx::simulation {
namespace {

timer_ticks constexpr minimum_frame_ticks{2}, realistic_minimum_ticks{4};
timer_ticks constexpr military_maximum_ticks{4}, maximum_frame_ticks{18};
unsigned int constexpr time_warp_multiplier{5};
float constexpr previous_frame_weight{3}, filter_scale{0.25f};
float constexpr physics_seconds_per_tick{0.02f};
timer_ticks constexpr maximum_projectile_step{10};

} // anonymous namespace

flight_model configured_flight_model(unsigned int const index) {
  std::array constexpr models{flight_model::arcade, flight_model::simple, flight_model::realistic, flight_model::military_spec};
  return models[index % models.size()];
}

flight_clock::flight_clock(timer_counter const initial_counter) : previous{initial_counter} {
}

void flight_clock::begin(timing_options const options) {
  if(pending) throw std::logic_error{"a flight frame is already waiting for its timer"};
  pending = options;
  accumulated = 0;
  realistic_extra_poll = false;
}

void flight_clock::resume(timer_counter const counter, timing_options const options) {
  previous = counter;
  pending = options;
  accumulated = 0;
  realistic_extra_poll = false;
}

std::optional<frame_timing> flight_clock::poll(timer_counter const counter, timing_options const options) {
  // Native input can change acceleration while this frame waits for the timer.
  // Replacing settings retains accumulated ticks and the Realistic extra poll.
  if(!pending) throw std::logic_error{"begin a flight frame before changing its timer settings"};
  pending = options;
  return poll(counter);
}

std::optional<frame_timing> flight_clock::poll(timer_counter const counter) {
  /// 2.05 file 0x8c43c: retain counter/word wrap, polling order and x87 storage boundaries
  if(!pending) throw std::logic_error{"begin a flight frame before supplying timer readings"};
  auto const elapsed{counter - previous};
  previous = counter;
  accumulated = static_cast<timer_ticks>(accumulated + elapsed);
  if(!realistic_extra_poll) {
    if(accumulated < minimum_frame_ticks) return std::nullopt;
    if(pending->model == flight_model::realistic) {
      // The native second loop is do/while: even an already-long frame reads the timer again.
      realistic_extra_poll = true;
      return std::nullopt;
    }
  } else if(accumulated < realistic_minimum_ticks) return std::nullopt;
  if(pending->model == flight_model::military_spec) accumulated = std::min(accumulated, military_maximum_ticks);
  accumulated = std::min(accumulated, maximum_frame_ticks);
  // Evaluate with enough precision, then round once at the original float store.
  filtered = static_cast<float>((static_cast<double>(filtered) * previous_frame_weight + accumulated) * filter_scale);
  double const seconds{accumulated * static_cast<double>(physics_seconds_per_tick)};
  if(pending->time_warp && pending->time_warp_allowed) accumulated = static_cast<timer_ticks>(accumulated * time_warp_multiplier);
  pending.reset();
  return frame_timing{
    .world_ticks{accumulated},
    .physics_seconds{seconds},
    .filtered_ticks{filtered},
  };
}

projectile_clock::projectile_clock(timer_ticks const elapsed) : remaining{elapsed} {
}
std::optional<timer_ticks> projectile_clock::next() {
  /// Native 0x71fd4 calls motion then collision per chunk, including one call for zero elapsed
  if(finished) return std::nullopt;
  if(math::signed_word(remaining) > maximum_projectile_step) {
    remaining = static_cast<timer_ticks>(remaining - maximum_projectile_step);
    return maximum_projectile_step;
  }
  finished = true;
  return remaining;
}

} // namespace tfx::simulation
