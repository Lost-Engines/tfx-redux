#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace tfx::math {

using angle = uint16_t;
inline constexpr unsigned angle_units_per_turn=65536;
constexpr int16_t signed_word(uint16_t const value) {
  return std::bit_cast<int16_t>(value);
}
constexpr double angle_radians(angle const value) {
  return signed_word(value)*(2*std::numbers::pi/angle_units_per_turn);
}
// The fixed-point callers still store Q15 words. Evaluate all sixteen angle
// bits first, then quantise only the amplitude at that existing interface.
// 32767 avoids overflowing the signed word at the positive cardinal peak.
inline int32_t sine_q15(int32_t const units) {
  return static_cast<int32_t>(std::sin(angle_radians(static_cast<angle>(units)))*32767);
}
inline int32_t cosine_q15(int32_t const units) {
  return static_cast<int32_t>(std::cos(angle_radians(static_cast<angle>(units)))*32767);
}

} // namespace tfx::math
