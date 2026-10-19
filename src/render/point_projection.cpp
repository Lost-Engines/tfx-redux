#include "point_projection.h"
#include <bit>
#include <algorithm>
#include <limits>

namespace tfx::render {
namespace {
int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }
}

std::optional<vec2<int16_t>> point_projection::project(vec3<int32_t> const &camera_point) const {
  // Native 0xc5759/0xc57d1 use a truncated reciprocal, then the signed high
  // product. Replacing this with direct division loses negative-pixel rounding.
  int constexpr minimum_depth{16}, maximum_depth{20000};
  int32_t const depth{camera_point.z >> 16};
  if(depth < minimum_depth || depth > maximum_depth) return std::nullopt;
  if(focal_distance) {
    double const z{camera_point.z / 65536.0};
    vec2d const projected{origin.x + camera_point.x / 65536.0 * *focal_distance / z,origin.y + camera_point.y / 65536.0 * *focal_distance / z};
    if(projected.x < -32768 || projected.x > 32767 || projected.y < -32768 || projected.y > 32767) return std::nullopt;
    return vec2<int16_t>{projected};
  }
  auto const reciprocal{static_cast<int32_t>((uint64_t{1} << 32) / static_cast<uint32_t>(depth))};
  auto const coordinate{[&](int32_t const value, int16_t const offset) {
    auto const high{(static_cast<int64_t>(value >> 8) * reciprocal) >> 32};
    return math::signed_word(static_cast<uint16_t>(high + offset));
  }};
  return vec2<int16_t>{coordinate(camera_point.x,origin.x),coordinate(camera_point.y,origin.y)};
}
vec3<int32_t> point_projection::camera_integer(vec3<int32_t> const &point) const {
  // Native 0xb9c9c subtracts/stores words before extending them. Camera
  // fractional position and coordinate bits above bit 15 are deliberately lost.
  vec3<int16_t> delta;
  for(unsigned int axis{0}; axis != 3; ++axis) delta[axis] = math::signed_word(static_cast<uint16_t>(static_cast<uint32_t>(point[axis]) - (static_cast<uint32_t>(position_q16[axis]) >> 16)));
  vec3<int32_t> transformed;
  for(unsigned int row{0}; row != 3; ++row) {
    uint32_t sum{0};
    for(unsigned int axis{0}; axis != 3; ++axis) sum += static_cast<uint32_t>(delta[axis]) * static_cast<uint32_t>(matrix.data[row * 3 + axis]);
    transformed[row] = wrap(sum);
  }
  return transformed;
}
std::optional<vec2<int16_t>> point_projection::integer(vec3<int32_t> const &point) const {
  return project(camera_integer(point));
}
bool point_projection::sphere_visible(vec3<int32_t> const &point, int16_t const radius) const {
  return camera_sphere_visible(camera_integer(point),radius);
}
bool point_projection::camera_sphere_visible(vec3<int32_t> const &camera, int16_t const radius) const {
  // Native 0xcbef0 / 0xb9c00: viewport setup patches these aperture limits
  // into the code. Word ABS/subtraction quirks are retained.
  int16_t constexpr near_depth{16}, far_depth{16384};
  auto const word{[](int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }};
  auto const absolute{[&](int16_t const value) { return value < 0 ? word(-static_cast<int32_t>(value)) : value; }};
  int16_t const horizontal_limit{word(maximum.x - origin.x)};
  int16_t const vertical_limit{word(std::max(static_cast<int32_t>(maximum.y) - origin.y,static_cast<int32_t>(origin.y)))};

  int16_t const extent{absolute(radius)};
  int16_t depth{word(camera.z >> 16)};
  if(depth < 0) {
    if(depth == std::numeric_limits<int16_t>::min()) return false;
    depth = absolute(depth);
    if(word(depth - extent) > near_depth) return false;
  }
  if(depth > far_depth) return false;
  auto const outside{[&](int32_t const coordinate, int16_t const limit) {
    int16_t const excess{word(absolute(word(coordinate >> 16)) - extent)};
    if(excess < 0) return false;
    if(excess >= depth) return true; // Also excludes zero before signed division.
    return static_cast<int32_t>(excess) * static_cast<int32_t>(projection_focal_length(opening)) / depth > limit;
  }};
  return !outside(camera.x,horizontal_limit) && !outside(camera.y,vertical_limit);
}
vec3<int32_t> point_projection::camera_fixed(vec3<int32_t> const &point_q16) const {
  // Native 0xb9c84/0xb9d14 keep the full Q16 subtraction. Each matrix product
  // contributes its signed high dword separately before the wrapped row sum.
  vec3<int32_t> delta;
  for(unsigned int axis{0}; axis != 3; ++axis) delta[axis] = wrap(static_cast<uint32_t>(point_q16[axis]) - static_cast<uint32_t>(position_q16[axis]));
  vec3<int32_t> transformed;
  for(unsigned int row{0}; row != 3; ++row) {
    uint32_t sum{0};
    for(unsigned int axis{0}; axis != 3; ++axis) {
      int32_t const coefficient{wrap(static_cast<uint32_t>(static_cast<uint16_t>(matrix.data[row * 3 + axis])) << 16)};
      sum += static_cast<uint32_t>((static_cast<int64_t>(coefficient) * delta[axis]) >> 32);
    }
    transformed[row] = wrap(sum);
  }
  return transformed;
}
std::optional<vec2<int16_t>> point_projection::fixed(vec3<int32_t> const &point_q16) const {
  return project(camera_fixed(point_q16));
}

} // namespace tfx::render
