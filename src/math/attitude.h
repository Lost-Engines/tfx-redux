#pragma once

#include "math/native.h"

namespace tfx::math {

enum class rotation_axis { pitch, heading, roll };
struct attitude {
  vec3<int32_t> right{32767,0,0};
  vec3<int32_t> down{0,32767,0};
  vec3<int32_t> forward{0,0,32767};
  void rotate(rotation_axis axis, angle amount);
  void normalise();
  bool align_down(vec3f const &direction);
  attitude transposed() const;
  vec3<angle> angles() const;
};

angle direction_angle(int32_t vertical, int32_t horizontal);
uint32_t integer_sqrt(uint32_t value);

} // namespace tfx::math
