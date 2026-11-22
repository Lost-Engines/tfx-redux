#pragma once

#include <algorithm>
#include "math/native.h"

namespace tfx::flight {

struct fuel_gauge {
  int16_t reading;
  int16_t smoothed_burn;
  constexpr void sample(int16_t const value) {
    // Native outer player callback 0x8fa23: clamp the new gauge, average its
    // drop with the previous signed-word estimate, then retain a minimum of two.
    int const current{std::max(0, static_cast<int>(value))};
    int const average{(static_cast<int>(smoothed_burn) + reading - current) / 2};
    int16_t constexpr minimum_burn{2};
    smoothed_burn = std::max(minimum_burn, math::signed_word(static_cast<uint16_t>(average)));
    reading = static_cast<int16_t>(current);
  }
};

} // namespace tfx::flight
