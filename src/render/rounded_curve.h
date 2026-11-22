#pragma once

#include <array>
#include <bit>
#include "math/native.h"

namespace tfx::render {

// DAF50's integer quarter-arc recurrence. Rounded quad corners use subdivision
// 2 (eight points); the standalone three-vertex form uses 3 (fourteen points).
template<unsigned int Subdivision>
auto rounded_arc(vec2<int32_t> const &previous, vec2<int32_t> const &corner, vec2<int32_t> const &next) {
  static_assert(Subdivision >= 1 && Subdivision <= 4);
  unsigned int constexpr fraction_bits{16}, quarter_turn_q16{0x19220};
  unsigned int constexpr point_count{((quarter_turn_q16 << Subdivision) >> fraction_bits) + 2};
  unsigned int constexpr first_shift{Subdivision * 2 + 3}, second_shift{Subdivision * 2 + 4};
  int64_t constexpr one{int64_t{1} << fraction_bits}, half{one / 2};
  auto const wrap{[](int64_t const value){ return std::bit_cast<int32_t>(static_cast<uint32_t>(value)); }};
  struct curve_axis {
    int32_t a;
    int32_t b;
    int32_t base;
  };
  std::array<curve_axis,2> state;
  for(unsigned int axis{0}; axis != 2; ++axis) {
    auto &s{state[axis]};
    s.a = wrap((static_cast<int64_t>(corner[axis]) - next[axis]) * one);
    s.b = wrap((static_cast<int64_t>(corner[axis]) - previous[axis]) * one);
    s.base = wrap(static_cast<int64_t>(previous[axis]) * one - s.a + half);
    auto delta{s.b >> first_shift};
    s.b = wrap(static_cast<int64_t>(s.b) - delta);
    delta >>= second_shift;
    s.b = wrap(static_cast<int64_t>(s.b) - delta);
    delta >>= first_shift;
    s.b = wrap(static_cast<int64_t>(s.b) - delta + (s.a >> (Subdivision + 1)));
  }
  std::array<vec2<int32_t>,point_count> result;
  for(unsigned int step{0}; step != point_count - 1; ++step) for(unsigned int axis{0}; axis != 2; ++axis) {
    auto &s{state[axis]};
    result[step][axis] = wrap(static_cast<int64_t>(s.base) + s.a) >> fraction_bits;
    s.b = wrap(static_cast<int64_t>(s.b) - (s.a >> Subdivision));
    s.a = wrap(static_cast<int64_t>(s.a) + (s.b >> Subdivision));
  }
  result.back() = next;
  return result;
}

} // namespace tfx::render
