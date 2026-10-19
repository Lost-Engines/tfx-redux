#pragma once

#include "platform/window.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "weapons/loadout.h"

namespace tfx::platform {

enum class arming_result { accepted, cancelled, closed };

arming_result arm_mission(window &window, resources::archive const &game, resources::executable_tables const &tables,
  weapons::armament &inventory, flight::player_aircraft aircraft, unsigned int default_fit, presentation::arming_entry entry = presentation::arming_entry::preflight);

} // namespace tfx::platform
