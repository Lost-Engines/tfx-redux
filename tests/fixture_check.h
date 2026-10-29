#pragma once

#include <cstdlib>
#include <fstream>
#include <istream>
#include <stdexcept>
#include <utility>

namespace tfx::test {
// Ordinary runs compare every expected value exactly. The explicit developer
// recording tool sets this variable to collect only changed expectation tokens;
// it never changes fixture inputs or suppresses malformed-input failures.
inline std::ofstream &fixture_report(char const *path) {
  static std::ofstream report{path};
  report.exceptions(std::ios::badbit|std::ios::failbit);
  return report;
}
template<class T> struct fixture_expectation {
  T value{};
  std::streampos begin{},end{};
  explicit fixture_expectation(std::istream &input) {
    input >> std::ws;
    begin=input.tellg();
    if(!(input >> value))throw std::runtime_error{"missing fixture expectation"};
    end=input.tellg();
  }
  bool matches(T const actual) const {
    if(auto const path=std::getenv("TFX_RECORD_TRIG_FIXTURE")) {
      if(value!=actual) {
        if(begin<0 || end<begin)throw std::runtime_error{"fixture expectation has no seekable extent"};
        fixture_report(path)<<begin<<' '<<end<<' '<<value<<' '<<actual<<'\n';
      }
      return true;
    }
    return value==actual;
  }
};
template<class T> bool fixture_matches(std::istream &input,T const actual) {
  return fixture_expectation<T>{input}.matches(actual);
}
} // namespace tfx::test
