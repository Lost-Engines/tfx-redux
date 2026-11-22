#pragma once

#include "math/native.h"

namespace tfx::render {

enum class model_surface {
  body,
  ground_shadow
};

vec3<int32_t> scale_model_cache(vec3<int32_t> const &origin, uint8_t scale);

// A submitted model owns its camera cache and the quantised view/body basis.
// Inspection uses its separate continuous world transform.
class model_transform {
private:
  mat3<int32_t> basis;
  vec3<int32_t> camera_origin;
public:
  model_transform(mat3<int32_t> const &view, vec3<math::angle> const &angles, vec3<int32_t> const &origin, model_surface surface = model_surface::body);
  mat3<int32_t> const &matrix() const;
  vec3d point(vec3d const &position, double projection_scale) const;
};

} // namespace tfx::render
