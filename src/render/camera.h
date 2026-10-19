#pragma once

#include "math/native.h"
#include "math/attitude.h"
#include "render/framebuffer.h"
#include "render/aperture.h"
#include "render/point_projection.h"

namespace tfx::render {

enum class projection_mode { continuous, original_model };

struct camera {
  vec3d position;
  vec3d right;
  vec3d down;
  vec3d forward;
  vec2d projection_origin{centre_x,centre_y};
  double focal_distance{focal_length};
  projection_mode projection{projection_mode::continuous};
  aperture opening{aperture::normal};
  std::optional<double> point_focal_distance{};
  double near_distance{16};
  double far_distance{20000};
  vec2<int16_t> viewport_minimum{0,0};
  vec2<int16_t> viewport_maximum{screen_width - 1,screen_height - 1};
  // Retain the original setter inputs as well as its derived basis. Other
  // native world callbacks can reuse the last installed camera verbatim.
  vec3<math::angle> view_angles{0,0,0};
  // Integer world/terrain anchor; chase smoothing can offset position from it.
  vec3<int32_t> world_anchor{0,0,0};
  point_projection projector() const;
  vec3d transform(vec3d const &world) const;
  vec2d project(vec3d const &camera_position, double projection_scale = 1) const;
};

camera aircraft_camera(vec3f const &position, math::attitude const &orientation);

camera orbit_camera(vec3<math::angle> const &angles, int32_t distance, vec2d const &projection_origin);

camera original_camera(vec3<int32_t> const &position_q16, mat3<int32_t> const &matrix, vec2d const &projection_origin, aperture opening = aperture::normal);

camera inspection_camera(vec3d const &position, double heading, double pitch);

} // namespace tfx::render
