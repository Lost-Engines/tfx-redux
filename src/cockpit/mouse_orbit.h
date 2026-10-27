#pragma once

#include "cockpit/view_camera.h"

namespace tfx::cockpit {

// A continuous mouse offset around the native camera's subject. Keeping this
// separate preserves the authored tracking, fly-past and keyboard camera laws.
class mouse_orbit {
  view_mode selected{view_mode::cockpit};
  double heading{0}, pitch{0};
public:
  static bool supports(view_mode mode);
  bool select(view_mode mode);
  void reset();
  void move(vec2d const &delta);
  render::camera apply(render::camera view, view_camera_context const &context,
                       missile_camera_pose const &last_missile) const;
};

} // namespace tfx::cockpit
