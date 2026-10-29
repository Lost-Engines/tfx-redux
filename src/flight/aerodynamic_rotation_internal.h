#pragma once

#include "flight/aerodynamic_motion.h"

namespace tfx::flight {

enum class aerodynamic_rotation { roll, pitch, yaw };
// Original 0x953ec/0x95440/0x95494 accept 65,536-unit angles, retain roll/pitch
// remainders. Redux applies single angle units instead of 64-unit table steps.
void rotate_aerodynamic_units(aerodynamic_motion &state, aerodynamic_rotation axis, double units);

} // namespace tfx::flight
