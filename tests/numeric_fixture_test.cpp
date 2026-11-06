#include "numeric_fixture.h"
#include <iostream>
#include <sstream>

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
uint64_t bits(double const value) { return std::bit_cast<uint64_t>(value); }
}

int main() try {
  using tfx::test::numeric_fixture_value;
  static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
  static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);
  require(numeric_fixture_value{uint64_t{1} << 62}.matches(uint64_t{1} << 62),"equal integers differ");
  require(!numeric_fixture_value{uint64_t{1} << 62}.matches((uint64_t{1} << 62) + 1),"integer state received floating tolerance");
  require(!numeric_fixture_value{true}.matches(0),"event flag received floating tolerance");
  require(numeric_fixture_value{1.0}.matches(bits(std::nextafter(1.0,2.0))),"adjacent double rejected");
  require(!numeric_fixture_value{1.0}.matches(bits(1.0 + 1e-10)),"large double error accepted");
  require(numeric_fixture_value{1e-15}.matches(bits(0.0)),"cancellation rounding rejected");
  require(!numeric_fixture_value{1e-10}.matches(bits(0.0)),"large absolute error accepted");
  // Original command fixture 13 cancels two forces near 19,600. Perturbing
  // either trig result by one representable step changes the residual by
  // about 3.64e-12, despite both evaluations being accurate at operand scale.
  double constexpr expected_lift{-5.331480957916938};
  double constexpr forward{24157.491258182854}, down{33517.82618323172};
  double constexpr sine{0.8111758096842762}, cosine{0.5848023647208167};
  double const lift{std::nextafter(sine,1.0) * forward - cosine * down};
  double constexpr lift_roundoff{4 * std::numeric_limits<double>::epsilon() * (forward + down)};
  require(!numeric_fixture_value{lift}.matches(bits(expected_lift)),"cancellation case no longer exercises operand bound");
  require(numeric_fixture_value::with_absolute_roundoff(lift,lift_roundoff).matches(bits(expected_lift)),"operand-scale rounding rejected");
  require(!numeric_fixture_value::with_absolute_roundoff(lift + 1e-9,lift_roundoff).matches(bits(expected_lift)),"excessive force error accepted");
  for(double const bound : {-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
    bool rejected{false};
    try { numeric_fixture_value::with_absolute_roundoff(0.0,bound); }
    catch(std::invalid_argument const &) { rejected=true; }
    require(rejected,"invalid absolute rounding bound accepted");
  }
  require(numeric_fixture_value{1.0f}.matches(std::bit_cast<uint32_t>(std::nextafter(1.0f,2.0f))),"adjacent float rejected");
  require(!numeric_fixture_value{1.0f}.matches(std::bit_cast<uint32_t>(1.01f)),"large float error accepted");
  require(!numeric_fixture_value{0.0f}.matches(uint64_t{1} << 32),"malformed float bits truncated");
  double constexpr inf{std::numeric_limits<double>::infinity()},nan{std::numeric_limits<double>::quiet_NaN()};
  require(numeric_fixture_value{nan}.matches(bits(nan)),"matching NaNs rejected");
  require(!numeric_fixture_value{nan}.matches(bits(1.0)),"finite result accepted as NaN");
  require(numeric_fixture_value{inf}.matches(bits(inf)),"matching infinity rejected");
  require(!numeric_fixture_value{inf}.matches(bits(-inf)),"opposite infinity accepted");
  require(!numeric_fixture_value{inf}.matches(bits(1.0)),"finite result accepted as infinity");
  std::stringstream encoded;
  encoded << numeric_fixture_value{1.5};
  require(encoded.str() == std::to_string(bits(1.5)),"diagnostic encoding changed");
  require(tfx::test::fixture_matches(encoded,numeric_fixture_value{1.5}),"encoded fixture did not round trip");
  bool rejected{false};
  try { std::istringstream empty; tfx::test::fixture_matches(empty,numeric_fixture_value{0.0}); }
  catch(std::runtime_error const &) { rejected=true; }
  require(rejected,"truncated fixture accepted");
  std::cout << "Portable numeric comparisons preserve integer state, bound rounding, and reject invalid fixtures\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
