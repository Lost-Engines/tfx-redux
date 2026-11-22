#pragma once

#include <cstdint>
#include <optional>

namespace tfx::simulation {

using timer_counter = uint32_t;
using timer_ticks = uint16_t;
unsigned int constexpr pit_input_hz{1193182}, pit_divisor{21845};
double constexpr timer_hz{static_cast<double>(pit_input_hz) / pit_divisor};

// Native codes differ from the four MODEL configuration-file indices.
enum class flight_model : uint8_t {
  arcade,
  military_spec,
  simple,
  realistic = 6
};
flight_model configured_flight_model(unsigned int index);

struct frame_timing {
  timer_ticks world_ticks;
  double physics_seconds;
  float filtered_ticks;
};
struct timing_options {
  flight_model model{flight_model::arcade};
  bool time_warp{false};
  bool time_warp_allowed{true};
};

class flight_clock {
private:
  timer_counter previous;
  timer_ticks accumulated{0};
  float filtered{4};
  std::optional<timing_options> pending;
  bool realistic_extra_poll{false};

public:
  explicit flight_clock(timer_counter initial_counter);
  void begin(timing_options options);
  void resume(timer_counter counter, timing_options options);
  std::optional<frame_timing> poll(timer_counter counter);
  std::optional<frame_timing> poll(timer_counter counter, timing_options options);
};

class projectile_clock {
private:
  timer_ticks remaining;
  bool finished{false};

public:
  explicit projectile_clock(timer_ticks elapsed);
  std::optional<timer_ticks> next();
};

} // namespace tfx::simulation
