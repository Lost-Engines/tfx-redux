#pragma once

#include "cockpit/instrument_assets.h"
#include "cockpit/view_camera.h"
#include "render/software_renderer.h"
#include "sensors/ground.h"

namespace tfx::cockpit {

view_pose designator_pose(sensors::designator const &state);
render::camera designator_display_camera(sensors::designator const &state,vec2d const &projection_origin);

struct designator_animation {
  int16_t search{0};
  int16_t tracking{0};
};

class designator_display {
private:
  instrument_assets const &assets;
  resources::indexed_image marker;
  designator_animation animation;

public:
  designator_display(instrument_assets const &assets, resources::archive const &game, designator_animation animation = {});
  designator_animation const &state() const;
  void draw(render::software_renderer &renderer, vec2<int16_t> const &point, bool tracking, render::framebuffer *coverage = nullptr);
};

} // namespace tfx::cockpit
