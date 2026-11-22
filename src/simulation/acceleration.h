#pragma once

namespace tfx::simulation {

struct time_acceleration {
  bool requested{false};                                                       // Native DATA 0x13248 survives temporary input inhibition.
  bool active{false};                                                          // DATA 0xe780 is recomputed before each player callback.

  bool toggle() {
    requested = !requested;
    return requested;
  }
  void resolve(bool const steering, bool const automatic_landing) {
    active = requested && !steering && !automatic_landing;
  }
  void cancel() {
    requested = active = false;
  }
};

} // namespace tfx::simulation
