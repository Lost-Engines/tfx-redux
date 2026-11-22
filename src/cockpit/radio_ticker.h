#pragma once

#include <optional>
#include <string>
#include <string_view>
#include "simulation/timing.h"

namespace tfx::cockpit {

struct radio_status {
  int16_t cursor;
  int16_t remainder;
  bool complete;
  bool history;
  bool persistent;
};
class radio_ticker {
private:
  std::string padding;
  std::string buffer;
  int16_t cursor{0};
  int16_t remainder{0};
  bool complete{true};
  bool history{false};
  bool persistent{false};

public:
  explicit radio_ticker(std::string_view padding);
  void clear();
  void show(std::string_view message, bool repeat = false);
  void replay();
  void stop();                                                                 // Native T cancellation marks idle without clearing history or the cursor.
  radio_status status() const;
  std::optional<std::string> advance(simulation::timer_ticks elapsed, bool time_warp = false);
};

} // namespace tfx::cockpit
