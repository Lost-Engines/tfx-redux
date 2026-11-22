#pragma once

#include "platform/window.h"
#include "cockpit/tactical_map.h"

namespace tfx::platform {

auto show_tactical_map(window &window, cockpit::tactical_map &map,
  cockpit::tactical_map_snapshot const &state, render::palette const &colours)->void;

} // namespace tfx::platform
