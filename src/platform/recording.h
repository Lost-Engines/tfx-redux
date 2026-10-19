#pragma once

#include "platform/window.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "world/scene.h"

namespace tfx::platform {

// WNN/press-room introduction followed by Redux video or legacy DOS model
// queues. The legacy path borrows the flight renderer and carried-model state.
void replay_recording(window &window, resources::archive const &game,
  resources::executable_tables const &tables, world::scene &scene,
  render::software_renderer &renderer, std::filesystem::path const &path,
  int16_t camera_y, int16_t ground_phase, render::dac_palette const &infrared, bool introduction = true);

} // namespace tfx::platform
