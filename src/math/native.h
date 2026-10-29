#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include "math/trigonometry.h"
#include "vectorstorm/vector/vector2.h"
#include "vectorstorm/vector/vector3.h"
#include "vectorstorm/matrix/matrix3.h"

namespace tfx::math {

int constexpr matrix_fraction_bits{15};
int constexpr matrix_unit{1 << matrix_fraction_bits};

vec2<angle> bearing(vec3<int32_t> const &from, vec3<int32_t> const &to);
int32_t distance_estimate(vec3<int32_t> const &from, vec3<int32_t> const &to);
void advance_position(vec3<int32_t> &position, vec3<angle> const &angles, int32_t distance, int32_t minimum_y, int32_t maximum_y);
int16_t distance_word(vec3<int16_t> const &offset);
vec3<int32_t> place_anchor(vec3<int32_t> const &origin, vec3<angle> const &angles, vec3<int16_t> const &anchor);
vec2<int16_t> rotate_words(vec2<int16_t> const &position, angle rotation);
mat3<int32_t> rotation_words(vec3<angle> const &angles);
mat3<int32_t> view_rotation_words(vec3<angle> const &angles);
mat3<int32_t> compose_model_basis(mat3<int32_t> const &view, mat3<int32_t> const &component);
vec3<int32_t> orbit_offset_q16(vec2<angle> const &angles, int32_t distance);
mat3d rotation(vec3<angle> const &angles);
vec3d rotate_tile(vec3d const &position, angle heading);

struct random_state {
  uint16_t low{0x5926};
  uint16_t high{0x3141};
  uint16_t next();
};

struct jitter_state {
  uint16_t x{0x3141};
  uint16_t y{0x5926};
  int16_t perturb(int16_t coordinate, unsigned int shift);
};

} // namespace tfx::math
