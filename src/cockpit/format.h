#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace tfx::cockpit::detail {

template<typename... Values>
std::string formatted(std::string const &format, Values const... values) {
  // Formats come only from the verified reference executable.
  std::array<char, 128> buffer{};
  int const length{std::snprintf(buffer.data(), buffer.size(), format.c_str(), values...)};
  if(length < 0 || static_cast<unsigned int>(length) >= buffer.size()) throw std::runtime_error{"HUD readout exceeds its display buffer"};
  return {buffer.data(), static_cast<unsigned int>(length)};
}
inline double decimal_value(double const value, unsigned int const precision) {
  // Watcom rounds the stored double after multiplying by 10^precision.
  // Its rounded decimal zero has no sign. Host printf instead rounds the
  // exact binary fraction: e.g. it prints 0.005 as "0.01", rather than "0.00".
  double const scale{precision == 2 ? 100.0 : precision == 1 ? 10.0 : 1.0};
  double const rounded{std::nearbyint(value * scale)};
  return rounded == 0 ? 0.0 : rounded / scale;
}
inline std::string decimal(std::string const &format, double const value, unsigned int const precision) {
  return formatted(format, decimal_value(value, precision));
}

} // namespace tfx::cockpit::detail
