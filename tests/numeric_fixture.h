#pragma once

#include "fixture_check.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ostream>
#include <iomanip>
#include <iostream>

namespace tfx::test {

// Keep original fixture bits and integer decisions. Only floating results may
// differ by rounding between the original x87 instructions and host libm.
class numeric_fixture_value {
  enum class format { integer, binary32, binary64 };
  uint64_t bits;
  format type;

  template<std::floating_point T> static bool close(T const actual, T const expected) {
    if(actual == expected) return true;
    if(std::isnan(actual) || std::isnan(expected)) return std::isnan(actual) && std::isnan(expected);
    if(!std::isfinite(actual) || !std::isfinite(expected)) return false;
    // The fixtures include chained trig, products and cancellation near zero.
    // Absolute tolerance is in the field's own units; relative tolerance
    // scales it for forces/moments. Float stores have a separate, tighter bound.
    T constexpr units{std::same_as<T,double> ? 128 : 4};
    T const bound{units * std::numeric_limits<T>::epsilon() * std::max({T{1},std::abs(actual),std::abs(expected)})};
    return std::abs(actual - expected) <= bound;
  }

public:
  template<std::integral T> numeric_fixture_value(T const value): bits{static_cast<uint64_t>(value)},type{format::integer} {}
  numeric_fixture_value(float const value): bits{std::bit_cast<uint32_t>(value)},type{format::binary32} {}
  numeric_fixture_value(double const value): bits{std::bit_cast<uint64_t>(value)},type{format::binary64} {}

  bool matches(uint64_t const expected) const {
    if(type == format::integer) return bits == expected;
    if(type == format::binary32) {
      if(expected > std::numeric_limits<uint32_t>::max()) return false;
      return close(std::bit_cast<float>(static_cast<uint32_t>(bits)),std::bit_cast<float>(static_cast<uint32_t>(expected)));
    }
    return close(std::bit_cast<double>(bits),std::bit_cast<double>(expected));
  }
  friend std::ostream &operator<<(std::ostream &out, numeric_fixture_value const value) { return out << value.bits; }
  friend bool fixture_matches(std::istream &input, numeric_fixture_value const actual) {
    fixture_expectation<uint64_t> const expected{input};
    if(auto const path=std::getenv("TFX_NUMERIC_AUDIT"); path && actual.type != format::integer && actual.bits != expected.value)
      fixture_report(path) << expected.begin << ' ' << (actual.type == format::binary32 ? 32 : 64) << ' ' << expected.value << ' ' << actual.bits << '\n';
    bool const ok{actual.type == format::integer ? expected.matches(actual.bits) : actual.matches(expected.value)};
    if(!ok) {
      std::cerr << "Numeric fixture mismatch at byte " << expected.begin
                << ": expected bits " << expected.value << ", actual bits " << actual.bits;
      if(actual.type == format::binary64)
        std::cerr << std::setprecision(17) << " (expected " << std::bit_cast<double>(expected.value)
                  << ", actual " << std::bit_cast<double>(actual.bits) << ')';
      std::cerr << '\n';
    }
    return ok;
  }
};

// Make the friend overload available to qualified calls as well as ADL.
bool fixture_matches(std::istream &input, numeric_fixture_value actual);

} // namespace tfx::test
