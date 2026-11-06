#include "aerodynamic_ground_internal.h"
#include <cmath>
#include <utility>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr gear_failure_multiplier{1.2}, metres_per_foot{.3048};
double constexpr contact_search_margin{-5}, launch_throttle{.6}, launch_force{700000};
double constexpr maximum_cable_length{600}, stopping_cable_length{200};
double constexpr maximum_stopping_speed{4}, minimum_stopping_speed{-4};
double constexpr gravity{9.81}, cockpit_bump_multiplier{1.2};
int constexpr feet_per_ground_step{8};

calculation wide(double const value) { return value; }
vec3d to_ned(aerodynamic_basis const &basis, vec3d const &body) {
  vec3d result;
  for(unsigned int axis{0}; axis != 3; ++axis)
    result[axis] = static_cast<double>(wide(basis.right[axis]) * body.y + wide(basis.forward[axis]) * body.x + wide(basis.down[axis]) * body.z);
  return result;
}
vec3d to_body(aerodynamic_basis const &basis, vec3d const &ned) {
  vec3d result;
  unsigned int axis{0};
  for(auto const *direction : {&basis.forward,&basis.right,&basis.down})
    result[axis++] = static_cast<double>(wide(direction->y) * ned.y + wide(direction->x) * ned.x + wide(direction->z) * ned.z);
  return result;
}

} // anonymous namespace

void update_aerodynamic_arrestor(aerodynamic_ground_state &state, aerodynamic_motion const &motion, aerodynamic_ground_parameters const &parameters, aerodynamic_ground_context const &context, bool const apply_moments) {
  auto &point{state.contact};
  auto const &basis{context.contact.basis};
  if(context.carrier_surface && state.arrestor == aerodynamic_arrestor_phase::free && context.arrestor_hook_down) {
    state.hook_offset_ned = to_ned(basis,parameters.hook_offset);
    for(unsigned int axis{0}; axis != 3; ++axis) state.hook_anchor_ned[axis] = static_cast<double>(wide(motion.pose.metres_ned[axis]) + state.hook_offset_ned[axis]);
    // The native height comparison has no branch: engagement is unconditional
    // here. The anchor then uses the carrier's terrain height exactly.
    state.hook_anchor_ned.z = static_cast<double>(wide(context.contact.ground_elevation_steps) * metres_per_foot * feet_per_ground_step);
    state.cable_length = 0;
    state.arrestor = aerodynamic_arrestor_phase::engaged;
  } else if(state.arrestor != aerodynamic_arrestor_phase::free) {
    if(context.gear_fraction == 0) state.arrestor = aerodynamic_arrestor_phase::free;
    if(state.cable_length > maximum_cable_length) state.arrestor = aerodynamic_arrestor_phase::free;
    else {
      for(unsigned int axis{0}; axis != 3; ++axis) state.hook_displacement[axis] = static_cast<double>(wide(motion.pose.metres_ned[axis]) + state.hook_offset_ned[axis] - state.hook_anchor_ned[axis]);
      auto const &displacement{state.hook_displacement};
      double const squared{static_cast<double>(wide(displacement.x) * displacement.x + wide(displacement.y) * displacement.y + wide(displacement.z) * displacement.z)};
      state.cable_length = static_cast<double>(std::sqrt(wide(squared)));
      calculation const inverse_length{1.0 / state.cable_length};
      for(unsigned int axis{0}; axis != 3; ++axis) {
        calculation const damping{wide(displacement[axis]) * inverse_length * motion.velocity_ned[axis] * parameters.arrestor_damping};
        state.hook_force_ned[axis] = static_cast<double>(-(damping + wide(parameters.arrestor_stiffness) * displacement[axis]));
      }
      auto const &velocity{motion.velocity_ned};
      state.cable_speed = static_cast<double>(std::sqrt(wide(velocity.x) * velocity.x + wide(velocity.y) * velocity.y + wide(velocity.z) * velocity.z));
      if(state.cable_length > stopping_cable_length || (state.cable_speed < maximum_stopping_speed && state.cable_speed > minimum_stopping_speed)) {
        calculation const inverse_seconds{1.0 / context.seconds};
        for(unsigned int axis{0}; axis != 3; ++axis) {
          calculation const inertia{wide(velocity[axis]) * inverse_seconds * context.contact.total_mass_kg + context.previous_ned_force[axis]};
          state.hook_force_ned[axis] = static_cast<double>(wide(state.hook_force_ned[axis]) - wide(displacement[axis]) * inverse_length * inertia);
        }
        state.arrestor = aerodynamic_arrestor_phase::stopped;
      }
      state.hook_force_body = to_body(basis,state.hook_force_ned);
      for(unsigned int axis{0}; axis != 3; ++axis) point.force[axis] = static_cast<double>(wide(state.hook_force_body[axis]) + point.force[axis]);
      if(apply_moments) {
        auto const &offset{parameters.hook_offset};
        auto const &force{state.hook_force_body};
        state.hook_moment.x = static_cast<double>(wide(offset.y) * force.z - wide(offset.z) * force.y);
        state.hook_moment.y = static_cast<double>(wide(offset.z) * force.x - wide(offset.x) * force.z);
        state.hook_moment.z = static_cast<double>(wide(offset.x) * force.y - wide(offset.y) * force.x);
        for(unsigned int axis{0}; axis != 3; ++axis) point.moment[axis] = static_cast<double>(wide(state.hook_moment[axis]) + point.moment[axis]);
      }
    }
  }
}

aerodynamic_ground_events update_aerodynamic_ground(aerodynamic_ground_state &state, aerodynamic_motion &motion, damage_state &damage, math::random_state &random, aerodynamic_ground_parameters const &parameters, aerodynamic_ground_context const &context) {
  aerodynamic_ground_events events;
  auto &point{state.contact};
  auto const &basis{context.contact.basis};
  // Native entry resets roll/pitch moments but retains the previous yaw moment.
  point.moment.x = point.moment.y = 0;
  point.force = vec3d{0,0,0};
  state.last_contact_kind = aerodynamic_contact_kind::retracted_wheel;
  auto &gear{damage.systems[std::to_underlying(aircraft_system::landing_gear)]};
  if(context.gear_fraction != 0) {
    state.last_contact_kind = aerodynamic_contact_kind::wheel;
    if(context.non_arcade_model) {
      if(context.metres_per_second > parameters.maximum_gear_speed && gear != system_condition::failed) gear = system_condition::intermittent;
      if(context.metres_per_second > wide(parameters.maximum_gear_speed) * gear_failure_multiplier && context.contact.gear_damage_enabled) gear = system_condition::failed;
    }
  }
  if(context.contact.terrain_present && motion.pose.metres_ned.z > context.contact.ground_elevation_steps * feet_per_ground_step * wide(metres_per_foot) + contact_search_margin) {
    auto point_context{context.contact};
    point_context.kind = state.last_contact_kind;
    point_context.rough_surface = context.contact.rough_surface && !context.carrier_surface;
    for(unsigned int index{0}; index != parameters.offsets.size(); ++index) {
      if(index == 3) point_context.kind = aerodynamic_contact_kind::body;
      point.offset_body = parameters.offsets[index];
      events.gear_impacts += update_aerodynamic_contact(point,motion,damage,random,parameters.contact,point_context);
    }
    state.last_contact_kind = aerodynamic_contact_kind::body;
    if(state.ground_steering && context.engine_factor.x != 0 && context.engine_factor.y != 0 && context.throttle >= launch_throttle) {
      point.force.x = static_cast<double>(wide(basis.forward.x) * launch_force + point.force.x);
      point.force.z = static_cast<double>(wide(basis.down.x) * launch_force + point.force.z);
    }
    update_aerodynamic_arrestor(state,motion,parameters,context,true);
    events.cockpit_bump = -wide(point.force.z) > wide(context.contact.total_mass_kg) * gravity * cockpit_bump_multiplier;
  }
  if(point.force.z < 0) {
    state.rollout_distance = static_cast<double>(wide(context.metres_per_second) * context.seconds + state.rollout_distance);
    state.landed = true;
  } else {
    state.ground_steering = state.landed = false;
    state.rollout_distance = 0;
  }
  if(motion.collision) {
    point.force = vec3d{0,0,0};
    point.moment = vec3d{0,0,0};
  }
  return events;
}

} // namespace tfx::flight
