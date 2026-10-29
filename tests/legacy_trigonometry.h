#pragma once

// Historical comparison for the benchmark only. Never included by game code.
#include "math/native.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace tfx::test_reference {
using math::angle;
inline constexpr unsigned angle_steps=1024;
inline std::array<int16_t, angle_steps> const &sine_table() {
  /// Reconstruct the original integer sine samples mathematically, including the negative-peak quirk
  static auto const table{[]{
    std::array<int16_t, angle_steps> result;
    for(unsigned int i{0}; i != angle_steps; ++i) result[i] = static_cast<int16_t>(std::sin(i * 2 * std::numbers::pi / angle_steps) * 32767);
    result[angle_steps * 3 / 4] = -32766;                                         // native table differs from trunc(sin * 32767) at this one entry
    return result;
  }()};
  return table;
}

inline angle direction_angle(int16_t const vertical, int16_t const horizontal) {
  /// Native 0xcdc2d quantises atan2 to 1,024 phases using truncated tangent thresholds
  if(!vertical) return horizontal < 0 ? 32768 : 0;
  // native thresholds are slightly irregular; regenerating a true tangent table changes returned phases
  static std::array<uint16_t,128> constexpr thresholds{
    201,603,1005,1407,1810,2212,2613,3017,
    3420,3824,4227,4632,5035,5442,5846,6252,
    6658,7065,7473,7882,8289,8698,9107,9518,
    9930,10343,10755,11168,11583,11999,12416,12834,
    13252,13672,14091,14515,14938,15362,15787,16213,
    16641,17072,17502,17934,18367,18803,19240,19677,
    20117,20559,21003,21448,21896,22344,22795,23246,
    23700,24158,24616,25077,25542,26005,26474,26944,
    27415,27890,28368,28848,29332,29817,30303,30793,
    31288,31785,32283,32786,33291,33800,34312,34829,
    35347,35868,36394,36923,37457,37994,38533,39078,
    39627,40181,40738,41299,41863,42433,43008,43589,
    44173,44761,45353,45954,46558,47166,47784,48404,
    49029,49660,50300,50944,51594,52249,52913,53583,
    54259,54942,55631,56330,57036,57749,58468,59196,
    59933,60680,61433,62195,62965,63744,64536,65335,
  };
  uint32_t const y{static_cast<uint32_t>(vertical < 0 ? -static_cast<int32_t>(vertical) : vertical)};
  uint32_t const x{static_cast<uint32_t>(horizontal < 0 ? -static_cast<int32_t>(horizontal) : horizontal)};
  auto const octant{[](uint32_t const small, uint32_t const large){
    auto const ratio{static_cast<uint16_t>((small << 16) / large)};
    return static_cast<unsigned int>(std::upper_bound(thresholds.begin(),thresholds.end(),ratio) - thresholds.begin()) * 64;
  }};
  unsigned int result{y == x ? 8192 : y > x ? 16384 - octant(x,y) : octant(y,x)};
  if(horizontal < 0) result = 32768 - result;
  if(vertical < 0) result = 0u - result;
  return static_cast<angle>(result);
}

} // namespace tfx::test_reference
