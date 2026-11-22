#pragma once

#include "flight/airframes.h"
#include "presentation/arming_controller.h"
#include "render/software_renderer.h"

namespace tfx::presentation {

void draw_arming_mission_incomplete(render::software_renderer &renderer, arming_assets const &assets);
void draw_arming_unavailable(render::software_renderer &renderer, arming_assets const &assets);
void draw_arming_store_label(render::software_renderer &renderer, std::string const &label, vec2<int> const &position);

flight::store_effects draw_arming_widgets(render::software_renderer &renderer, arming_assets const &assets,
  arming_state const &state, weapons::armament const &inventory, std::span<weapons::definition const> weapons,
  flight::airframe const &profile, double previous_drag);

} // namespace tfx::presentation
