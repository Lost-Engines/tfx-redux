#include "aerodynamic_forces.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr gravity{9.81}, fuel_roll_inertia{.004166666666666667};
double constexpr refuelling_level_moment{10000};

calculation native_asin(double const value) {
  // Detected 387 path: original 0xd242c then 0xd24a7's direct FPATAN.
  calculation const x{value};
  return std::atan2(x,std::sqrt(1.0 - x * x));
}

} // anonymous namespace

aerodynamic_force_result update_aerodynamic_forces(aerodynamic_motion &state, aerodynamic_force_context const &context) {
  auto const &basis{context.basis};
  auto const &loads{context.loads};
  auto const &inertia{context.inertia};
  aerodynamic_force_result result;
  vec3d const vertical{basis.forward.z,basis.right.z,basis.down.z};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    calculation const applied{static_cast<calculation>(loads.aerodynamic_force[axis]) + loads.engine_force[axis] + loads.ground_force[axis]};
    if(axis == 2) {
      result.g_load = static_cast<float>(-applied / (static_cast<calculation>(inertia.mass_kg) * gravity) + static_cast<calculation>(state.angular_rate.x) * state.angular_rate.x * context.cockpit_upward_metres / gravity);
    }
    // Down force is stored and reloaded after the G calculation; F/R are not.
    calculation const force{axis == 2 ? static_cast<double>(applied) : applied};
    result.body_force[axis] = static_cast<double>(force + static_cast<calculation>(vertical[axis]) * inertia.mass_kg * gravity);
    result.body_moment[axis] = static_cast<double>(static_cast<calculation>(loads.aerodynamic_moment[axis]) + loads.engine_moment[axis] + loads.ground_moment[axis]);
  }
  for(unsigned int axis{0}; axis != 3; ++axis) result.ned_force[axis] = static_cast<double>(static_cast<calculation>(basis.right[axis]) * result.body_force.y + static_cast<calculation>(basis.forward[axis]) * result.body_force.x + static_cast<calculation>(basis.down[axis]) * result.body_force.z);
  if(context.refuelling_connected) result.body_moment.x = static_cast<double>(result.body_moment.x - native_asin(basis.right.z) * refuelling_level_moment);
  if(context.fully_arrested) {
    // The native stop clears horizontal force/velocity, retaining NED down.
    result.ned_force.x = result.ned_force.y = 0;
    state.velocity_ned.x = state.velocity_ned.y = 0;
    state.air_velocity_body = vec3d{0,0,0};
    unsigned int axis{0};
    for(auto const *direction : {&basis.forward,&basis.right,&basis.down}) state.ground_velocity_body[axis++] = static_cast<double>(0.0 * direction->y + 0.0 * direction->x);
  }
  calculation const inverse_mass{1.0 / inertia.mass_kg};
  for(unsigned int axis{0}; axis != 3; ++axis) state.acceleration_ned[axis] = static_cast<double>(static_cast<calculation>(result.ned_force[axis]) * inverse_mass);
  state.angular_acceleration.x = static_cast<double>(static_cast<calculation>(result.body_moment.x) / (static_cast<calculation>(inertia.fuel_kg) * inertia.fuel_kg * fuel_roll_inertia + inertia.roll_moment));
  state.angular_acceleration.y = static_cast<double>(static_cast<calculation>(result.body_moment.y) / inertia.pitch_moment);
  return result;
}

} // namespace tfx::flight
