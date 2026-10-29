#include "attitude.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace tfx::math {
namespace {

int32_t wrap(int64_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value));
}
int32_t component(int64_t const value) {
  return signed_word(static_cast<uint16_t>(wrap(value) >> matrix_fraction_bits));
}
vec3<int32_t> quantise(vec3<int64_t> const &value) {
  return {component(value.x),component(value.y),component(value.z)};
}
vec3<int32_t> unit_cross(vec3<int32_t> const &first, vec3<int32_t> const &second) {
  auto const crossed{quantise(vec3<int64_t>{first}.cross(vec3<int64_t>{second}))};
  auto const squared{vec3<int64_t>{crossed}.length_sq()};
  auto const length{integer_sqrt(static_cast<uint32_t>(wrap(squared) >> 2)) + 1};
  int32_t const divisor{signed_word(static_cast<uint16_t>(length))};
  if(!divisor) throw std::runtime_error{"degenerate native attitude normalisation"};
  return {(crossed.x * (1 << 14)) / divisor,(crossed.y * (1 << 14)) / divisor,(crossed.z * (1 << 14)) / divisor};
}

} // anonymous namespace

uint32_t integer_sqrt(uint32_t const value) {
  /// Return the native integer square-root result without relying on libm rounding at square boundaries
  auto root{static_cast<uint32_t>(std::sqrt(static_cast<double>(value)))};
  while(static_cast<uint64_t>(root) * root > value) --root;
  while(static_cast<uint64_t>(root + 1) * (root + 1) <= value) ++root;
  return root;
}

angle direction_angle(int32_t const vertical, int32_t const horizontal) {
  // Keep all sixteen stored angle bits; no tangent thresholds or phase bins.
  if(!vertical && !horizontal) return 0;
  auto const units{std::atan2(static_cast<double>(vertical),static_cast<double>(horizontal))*(angle_units_per_turn/(2*std::numbers::pi))};
  return static_cast<angle>(std::lround(units));
}

void attitude::rotate(rotation_axis const axis, angle const amount) {
  /// Native local rotations 0xccf4c/0xcd058/0xcd15c retain 32-bit products and signed-word stores
  int64_t const sine{sine_q15(amount)}, cosine{cosine_q15(amount)};
  auto *first{axis == rotation_axis::pitch ? &down : &right};
  auto *second{axis == rotation_axis::roll ? &down : &forward};
  auto rotated_first{quantise(vec3<int64_t>{*first} * cosine - vec3<int64_t>{*second} * sine)};
  auto rotated_second{quantise(vec3<int64_t>{*second} * cosine + vec3<int64_t>{*first} * sine)};
  *first = std::move(rotated_first);
  *second = std::move(rotated_second);
}
void attitude::normalise() {
  /// Native 0xccd3c rebuilds and normalises successive columns using cross products and integer division
  right = unit_cross(down,forward);
  down = unit_cross(forward,right);
  forward = unit_cross(right,down);
}
bool attitude::align_down(vec3f const &direction) {
  // Native 0x8e86c and 0x91c48 build a down axis, then right/down cross
  // products. Their float stores differ from the integer normalise() path.
  using calculation = long double;
  float constexpr normalised_word{3.0518509447574615e-05f}, word_scale{32767};
  double constexpr minimum_length{.001};
  auto const length{[](vec3f const &vector) {
    calculation const x{vector.x}, y{vector.y}, z{vector.z};
    return std::sqrt(x * x + y * y + z * z);
  }};
  auto const normalised{[](int32_t const value) { return static_cast<calculation>(signed_word(static_cast<uint16_t>(value))) * normalised_word; }};
  auto const words{[&](vec3f const &vector, float const magnitude) {
    calculation const inverse{1.0L / magnitude};
    auto const component{[&](float const value) { return static_cast<int32_t>(signed_word(static_cast<uint16_t>(static_cast<int32_t>(static_cast<calculation>(value) * inverse * word_scale)))); }};
    return vec3<int32_t>{component(vector.x),component(vector.y),component(vector.z)};
  }};
  auto const crossed{[&](vec3<int32_t> const &a, vec3<int32_t> const &b) {
    vec3f result;
    for(unsigned int axis{0}; axis != 3; ++axis) {
      unsigned int const next{(axis + 1) % 3}, last{(axis + 2) % 3};
      float const first{static_cast<float>(normalised(a[next]))};
      float const product{static_cast<float>(normalised(b[last]) * first)};
      float const second{static_cast<float>(normalised(a[last]))};
      result[axis] = static_cast<float>(product - normalised(b[next]) * second);
    }
    return result;
  }};
  calculation const down_length{length(direction)};
  if(down_length < minimum_length) return false;
  auto const new_down{words(direction,static_cast<float>(down_length))};
  auto const right_direction{crossed(new_down,forward)};
  calculation const right_length{length(right_direction)};
  if(right_length < minimum_length) return false;
  auto const new_right{words(right_direction,static_cast<float>(right_length))};
  auto const forward_direction{crossed(new_right,new_down)};
  vec3<int32_t> new_forward;
  for(unsigned int axis{0}; axis != 3; ++axis) new_forward[axis] = signed_word(static_cast<uint16_t>(static_cast<int32_t>(static_cast<calculation>(forward_direction[axis]) * word_scale)));
  right = new_right;
  down = new_down;
  forward = new_forward;
  return true;
}
attitude attitude::transposed() const {
  return {
    .right{right.x,down.x,forward.x},
    .down{right.y,down.y,forward.y},
    .forward{right.z,down.z,forward.z},
  };
}
vec3<angle> attitude::angles() const {
  // Recover full-word pitch, heading and roll from the existing stored basis.
  auto const horizontal_length{integer_sqrt(static_cast<uint32_t>(static_cast<int64_t>(forward.x) * forward.x + static_cast<int64_t>(forward.z) * forward.z))};
  auto const pitch{static_cast<angle>(-direction_angle(-forward.y,static_cast<int32_t>(horizontal_length)))};
  auto const heading{direction_angle(forward.x,forward.z)};
  int32_t const roll_horizontal{wrap(static_cast<int64_t>(right.x) * forward.z - static_cast<int64_t>(right.z) * forward.x) >> 15};
  return {pitch,heading,static_cast<angle>(-direction_angle(right.y,roll_horizontal))};
}

} // namespace tfx::math
