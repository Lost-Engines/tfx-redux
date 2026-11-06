#include "aerodynamic_start.h"

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr normalised_word{3.051850947599719e-05}, metres_per_world_unit{2.4384};
double constexpr gravity{9.81}, suspension_points{3}, metres_per_foot{.3048};
double constexpr nighthawk_ground_offset{.03600000000000003}, initial_seconds{.01};
float constexpr metres_per_fixed_speed{.4762499928474426f};
int constexpr feet_per_ground_step{8};
int16_t constexpr carrier_height_limit{-10};

calculation wide(double const value) { return value; }
calculation normalised(int32_t const value) {
  return wide(math::signed_word(static_cast<uint16_t>(value))) * normalised_word;
}

} // anonymous namespace

void initialise_aerodynamic_motion(aerodynamic_start_state &state, aerodynamic_airframe &airframe, loadout const &stores, aerodynamic_start_context const &context) {
  state.mass_kg = static_cast<double>(wide(airframe.empty_mass_kg) + state.fuel_kg + state.stores.mass_kg);
  auto &motion{state.motion};
  auto const &body{context.orientation};
  motion.pose.frame = {
    .forward{body.forward.z,body.forward.x,body.forward.y},
    .right{body.right.z,body.right.x,body.right.y},
    .down{body.down.z,body.down.x,body.down.y},
  };
  // Preserve this initializer's authored component order. Its cockpit offsets
  // are applied cyclically relative to the later 0x95aa0 view export.
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const forward{static_cast<double>(normalised(body.forward[axis]) * airframe.viewpoint.forward)};
    calculation const offset{forward - normalised(body.down[axis]) * airframe.viewpoint.upward};
    motion.pose.metres_ned[axis] = static_cast<double>((wide(context.position[(axis + 2) % 3]) - offset) * metres_per_world_unit);
  }
  if(context.landed) {
    state.stores = accumulate_stores(airframe.stores_and_mounts.stores,stores);
    if(context.cold_start) {
      state.fuel_kg = state.stores.fuel_capacity;
      state.engine_factor = vec2d{0,0};
      if(airframe.stores_and_mounts.stores.aircraft == player_aircraft::raptor && context.terrain_present && context.ground_elevation_steps < carrier_height_limit) {
        airframe.use_carrier_setup();
        state.ground_steering = true;
      }
    }
    calculation const inverse_spring{1.0 / (wide(airframe.ground.contact.spring_stiffness) * suspension_points)};
    calculation const down{wide(context.ground_elevation_steps) * metres_per_foot * feet_per_ground_step - airframe.ground.offsets[1].z};
    motion.pose.metres_ned.z = static_cast<double>(wide(state.mass_kg) * gravity * inverse_spring + down);
    if(airframe.stores_and_mounts.stores.aircraft == player_aircraft::nighthawk) motion.pose.metres_ned.z = static_cast<double>(wide(motion.pose.metres_ned.z) + nighthawk_ground_offset);
    motion.angular_rate.x = motion.angular_rate.y = 0;
    motion.angular_acceleration = vec2d{0,0};
    motion.acceleration_ned = vec3d{0,0,0};
    motion.velocity_ned = vec3d{0,0,0};
  }
  motion.ground_velocity_body = vec3d{static_cast<double>(wide(context.fixed_speed) * metres_per_fixed_speed),0,0};
  state.track_heading = math::signed_word(context.heading);
  for(unsigned int axis{0}; axis != 3; ++axis) {
    double const right{static_cast<double>(normalised(motion.pose.frame.right[axis]) * motion.ground_velocity_body.y)};
    double const forward{static_cast<double>(normalised(motion.pose.frame.forward[axis]) * motion.ground_velocity_body.x + right)};
    motion.velocity_ned[axis] = static_cast<double>(normalised(motion.pose.frame.down[axis]) * motion.ground_velocity_body.z + forward);
  }
  state.stores = accumulate_stores(airframe.stores_and_mounts.stores,stores);
  state.g_load = 0;
  state.seconds = initial_seconds;
}

} // namespace tfx::flight
