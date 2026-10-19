#include "camera.h"
#include <cmath>

namespace tfx::render {

point_projection camera::projector() const {
  point_projection result{
    .matrix{},
    .position_q16{vec3<int32_t>{vec3<uint32_t>{vec3<int64_t>{position * 65536}}}},
    .origin{static_cast<int16_t>(projection_origin.x),static_cast<int16_t>(projection_origin.y)},
    .maximum{vec2<int16_t>{viewport_maximum}},
    .opening{opening},
    .focal_distance{point_focal_distance},
  };
  for(unsigned int row{0}; row != 3; ++row) {
    auto const &axis{row == 0 ? right : row == 1 ? down : forward};
    for(unsigned int column{0}; column != 3; ++column) result.matrix.data[row * 3 + column] = static_cast<int32_t>(axis[column] * math::matrix_unit);
  }
  return result;
}

vec3d camera::transform(vec3d const &world) const {
  auto const relative{world - position};
  return {relative.dot(right), relative.dot(down), relative.dot(forward)};
}
vec2d camera::project(vec3d const &point, double const projection_scale) const {
  if(projection == projection_mode::original_model) {
    // Native 0xb9434: signed Q16 X/Y shifted by eight (nine in missile-nose
    // view), divided by the signed
    // integer depth, then added to the viewport origin. IDIV truncates toward zero.
    // Camera-space coordinates enter the native loop as signed Q16. Restore
    // that grid after model-unit conversion; e.g. a 1/400 scale can otherwise
    // turn an exact depth of 29 into 28.999999999999996 before the integer divide.
    auto const q16{[projection_scale](double const value) { return std::nearbyint(value / projection_scale * 65536); }};
    double const depth{std::floor(q16(point.z) / 65536)};
    double const divisor{static_cast<double>(1u << projection_shift(opening))};
    return projection_origin + vec2d{std::trunc(std::floor(q16(point.x) / divisor) / depth),std::trunc(std::floor(q16(point.y) / divisor) / depth)};
  }
  return {projection_origin.x + point.x * focal_distance / point.z,projection_origin.y + point.y * focal_distance / point.z};
}

camera aircraft_camera(vec3f const &position, math::attitude const &orientation) {
  double constexpr q15_unit{32767};
  return {
    .position{vec3d{position}},
    .right{vec3d{orientation.right} / q15_unit},
    .down{vec3d{orientation.down} / q15_unit},
    .forward{vec3d{orientation.forward} / q15_unit},
  };
}

camera orbit_camera(vec3<math::angle> const &angles, int32_t const distance, vec2d const &projection_origin) {
  auto const position{math::orbit_offset_q16({angles.x,angles.y},distance)};
  auto result{original_camera(position,math::view_rotation_words(angles),projection_origin)};
  result.view_angles = vec3<math::angle>{angles};
  return result;
}

camera original_camera(vec3<int32_t> const &position_q16, mat3<int32_t> const &matrix, vec2d const &projection_origin, aperture const opening) {
  return {
    .position{vec3d{position_q16} / 65536},
    .right{vec3d{vec3<int32_t>{matrix.data[0],matrix.data[1],matrix.data[2]}} / math::matrix_unit},
    .down{vec3d{vec3<int32_t>{matrix.data[3],matrix.data[4],matrix.data[5]}} / math::matrix_unit},
    .forward{vec3d{vec3<int32_t>{matrix.data[6],matrix.data[7],matrix.data[8]}} / math::matrix_unit},
    .projection_origin{vec2d{projection_origin}},
    .focal_distance{static_cast<double>(projection_focal_length(opening))},
    .projection{projection_mode::original_model},
    .opening{opening},
    .world_anchor{position_q16.x >> 16,position_q16.y >> 16,position_q16.z >> 16},
  };
}

camera inspection_camera(vec3d const &position, double const heading, double const pitch) {
  /// A free research camera, deliberately separate from the player's fixed-point pose handoff
  double const sh{std::sin(heading)}, ch{std::cos(heading)}, sp{std::sin(pitch)}, cp{std::cos(pitch)};
  return {
    .position{vec3d{position}},
    .right{ch,0,-sh},
    .down{sh * sp,cp,ch * sp},
    .forward{sh * cp,-sp,ch * cp},
  };
}

} // namespace tfx::render
