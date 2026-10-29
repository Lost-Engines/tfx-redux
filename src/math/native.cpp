#include "native.h"
#include <algorithm>
#include <cmath>
#include "math/attitude.h"

namespace tfx::math {

vec2<angle> bearing(vec3<int32_t> const &from, vec3<int32_t> const &to) {
  // Retain wrapped world differences and Chebyshev pitch distance; bearings use full-word atan2.
  auto const delta{[](int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }};
  auto const magnitude{[&](int32_t const value) { return value < 0 ? delta(0,value) : value; }};
  int32_t const x{delta(to.x,from.x)}, y{delta(to.y,from.y)}, z{delta(to.z,from.z)};
  return {direction_angle(y,std::max(magnitude(x),magnitude(z))),direction_angle(x,z)};
}

int32_t distance_estimate(vec3<int32_t> const &from, vec3<int32_t> const &to) {
  // Native 0x7d208: largest absolute component plus one quarter of the other two, with dword wrapping.
  auto const magnitude{[](uint32_t const value) { return std::bit_cast<int32_t>(std::bit_cast<int32_t>(value) < 0 ? 0u - value : value); }};
  vec3<int32_t> const offset{magnitude(static_cast<uint32_t>(from.x) - static_cast<uint32_t>(to.x)),magnitude(static_cast<uint32_t>(from.y) - static_cast<uint32_t>(to.y)),magnitude(static_cast<uint32_t>(from.z) - static_cast<uint32_t>(to.z))};
  int32_t const longest{std::max({offset.x,offset.y,offset.z})};
  uint32_t const remainder{static_cast<uint32_t>(offset.x) + static_cast<uint32_t>(offset.y) + static_cast<uint32_t>(offset.z) - static_cast<uint32_t>(longest)};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(longest) + static_cast<uint32_t>(std::bit_cast<int32_t>(remainder) >> 2));
}

void advance_position(vec3<int32_t> &position, vec3<angle> const &angles, int32_t const distance, int32_t const minimum_y, int32_t const maximum_y) {
  // Shared arithmetic of native 0xc0560 (Q8 actors) and 0xc03f4 (integer camera, signed-word distance).
  int32_t const pitch{-static_cast<int32_t>(angles.x)}, heading{-static_cast<int32_t>(angles.y)};
  int32_t const cp{cosine_q15(pitch)};
  vec3<int16_t> const direction{
    signed_word(static_cast<uint16_t>(-((sine_q15(heading) * cp) >> 15))),
    signed_word(static_cast<uint16_t>(-sine_q15(pitch))),
    signed_word(static_cast<uint16_t>((cosine_q15(heading) * cp) >> 15)),
  };
  for(unsigned int axis{0}; axis != 3; ++axis) {
    int32_t const product{std::bit_cast<int32_t>(static_cast<uint32_t>(direction[axis]) * static_cast<uint32_t>(distance))};
    position[axis] = std::bit_cast<int32_t>(static_cast<uint32_t>(position[axis]) + static_cast<uint32_t>(product >> 15));
  }
  position.y = std::clamp(position.y,minimum_y,maximum_y);
}

int16_t distance_word(vec3<int16_t> const &offset) {
  /// Native 0xcbe94 / 0xd14b4: floor of the unsigned squared magnitude, returned in a signed word.
  auto const square{[](int32_t const value) { return static_cast<uint32_t>(value * value); }};
  auto const squared{square(offset.x) + square(offset.y) + square(offset.z)};
  return signed_word(static_cast<uint16_t>(std::sqrt(static_cast<double>(squared))));
}

vec2<int16_t> rotate_words(vec2<int16_t> const &position, angle const rotation) {
  // Native 0xcc364/0xcc390: signed-word inputs, Q15 sums, arithmetic shifts, then word stores.
  int32_t const sine{sine_q15(rotation)}, cosine{cosine_q15(rotation)};
  return {
    signed_word(static_cast<uint16_t>((position.x * cosine - position.y * sine) >> matrix_fraction_bits)),
    signed_word(static_cast<uint16_t>((position.x * sine + position.y * cosine) >> matrix_fraction_bits)),
  };
}

mat3<int32_t> rotation_words(vec3<angle> const &angles) {
  /// Native component generator, 2.05 file 0xdad88; stored words are column-major
  auto const sine{sine_q15}, cosine{cosine_q15};
  auto const multiply{[](int32_t const a, int32_t const b){ return (a * b) >> matrix_fraction_bits; }};
  int const pitch{angles.x}, yaw{angles.y}, roll{angles.z};
  auto const sp{sine(pitch)}, s{sine(yaw + roll)}, t{sine(yaw - roll)}, c{cosine(yaw + roll)}, d{cosine(yaw - roll)};
  mat3<int32_t> result;
  result.data = {
    (multiply(d - c, sp) + c + d) >> 1,
    -(sine(pitch + roll) - sine(pitch - roll)) >> 1,
    (multiply(s - t, sp) - s - t) >> 1,
    (-multiply(s + t, sp) + s - t) >> 1,
    (cosine(pitch + roll) + cosine(pitch - roll)) >> 1,
    -((multiply(c + d, sp) + d - c) >> 1),
    (sine(pitch + yaw) - sine(pitch - yaw)) >> 1,
    sp,
    (cosine(pitch - yaw) + cosine(pitch + yaw)) >> 1,
  };
  for(auto &value : result.data) value = signed_word(static_cast<uint16_t>(value));
  return result;
}

mat3<int32_t> compose_model_basis(mat3<int32_t> const &view, mat3<int32_t> const &component) {
  // Native B972A/B9895: row-major view times column-major component,
  // wrapping each dword dot product before its signed Q15 word store.
  mat3<int32_t> result;
  for(unsigned int row{0}; row != 3; ++row) for(unsigned int column{0}; column != 3; ++column) {
    uint32_t sum{0};
    for(unsigned int axis{0}; axis != 3; ++axis) sum += static_cast<uint32_t>(view.data[row * 3 + axis]) * static_cast<uint32_t>(component.data[column * 3 + axis]);
    result.data[row * 3 + column] = signed_word(static_cast<uint16_t>(std::bit_cast<int32_t>(sum) >> matrix_fraction_bits));
  }
  return result;
}

mat3<int32_t> view_rotation_words(vec3<angle> const &angles) {
  // Native 0xdabd8 constructs the view matrix separately from the component matrix.
  // Its ordered shifts differ by a unit in some entries; negating an Euler matrix loses that distinction.
  auto const sine{sine_q15}, cosine{cosine_q15};
  int const pitch{angles.x}, yaw{angles.y}, roll{angles.z};
  auto const sp{sine(pitch)}, s{sine(yaw + roll)}, t{sine(yaw - roll)}, c{cosine(yaw + roll)}, d{cosine(yaw - roll)};
  auto const multiply{[](int32_t const a, int32_t const b) { return (a * b) >> matrix_fraction_bits; }};
  // This setter stores rows, unlike the component generator's column-major output.
  mat3<int32_t> matrix;
  matrix.data = {
    (-multiply(d - c,sp) + c + d) >> 1,
    (sine(pitch + roll) - sine(pitch - roll)) >> 1,
    (multiply(s - t,sp) + s + t) >> 1,
    -((multiply(s + t,sp) + s - t) >> 1),
    (cosine(pitch + roll) + cosine(pitch - roll)) >> 1,
    (multiply(c + d,sp) + c - d) >> 1,
    -((sine(pitch + yaw) - sine(pitch - yaw)) >> 1),
    -sp,
    (cosine(pitch - yaw) + cosine(pitch + yaw)) >> 1,
  };
  for(auto &value : matrix.data) value = signed_word(static_cast<uint16_t>(value));
  return matrix;
}

vec3<int32_t> orbit_offset_q16(vec2<angle> const &angles, int32_t const distance) {
  auto const sine{sine_q15}, cosine{cosine_q15};
  int const pitch{angles.x}, yaw{angles.y};
  // Keep the original signed Q16 orbit coordinates, without the table-mask
  // complement that added an unwanted one-sample offset at every angle.
  auto const product{[](int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }};
  int const reverse_pitch{-pitch}, reverse_yaw{-yaw};
  int32_t const horizontal{product(cosine(reverse_pitch),distance) >> matrix_fraction_bits};
  auto const doubled{[&](int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(0u - static_cast<uint32_t>(product(a,b)) * 2u); }};
  return {doubled(sine(reverse_yaw),horizontal),doubled(sine(reverse_pitch),distance),doubled(cosine(reverse_yaw),horizontal)};
}

vec3<int32_t> place_anchor(vec3<int32_t> const &origin, vec3<angle> const &angles, vec3<int16_t> const &anchor) {
  // Native 0xb81de adds each signed high-word product separately. One dot product followed by a shift loses its rounding rules.
  auto result{vec3<int32_t>{origin}};
  auto const matrix{rotation_words(angles)};
  for(unsigned int row{0}; row != 3; ++row) for(unsigned int column{0}; column != 3; ++column) {
    int32_t const term{(static_cast<int32_t>(anchor[column]) * matrix.data[column * 3 + row]) >> 16};
    result[row] = std::bit_cast<int32_t>(static_cast<uint32_t>(result[row]) + static_cast<uint32_t>(term));
  }
  return result;
}

mat3d rotation(vec3<angle> const &angles) {
  return mat3d{rotation_words(angles)} / static_cast<double>(matrix_unit);
}

vec3d rotate_tile(vec3d const &position, angle const heading) {
  /// The terrain loader selects exact quarter-turn callbacks, not the component sine matrix
  switch(heading >> 14) {
  case 1: return {position.z, position.y, -position.x};
  case 2: return {-position.x, position.y, -position.z};
  case 3: return {-position.z, position.y, position.x};
  default: return vec3d{position};
  }
}

uint16_t random_state::next() {
  /// Original ADD/ADC generator at 2.05 file 0xc98a3, returning the updated high word
  auto const sum{static_cast<unsigned int>(high) + low};
  high = static_cast<uint16_t>(sum);
  low = static_cast<uint16_t>(low + high + (sum > 65535));
  return high;
}

int16_t jitter_state::perturb(int16_t const coordinate, unsigned int const shift) {
  /// Native vertex jitter's distinct ADD/ADC ordering at 0xbc13a, followed by word SHL/SAR
  auto const sum{static_cast<unsigned int>(x) + y};
  x = static_cast<uint16_t>(sum);
  y = static_cast<uint16_t>(y + x + (sum > 65535));
  auto const count{shift & 31};
  int const delta{signed_word(static_cast<uint16_t>(static_cast<unsigned int>(x) << count)) >> count};
  return signed_word(static_cast<uint16_t>(coordinate + delta));
}

} // namespace tfx::math
