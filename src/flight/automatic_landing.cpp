#include "automatic_landing.h"
#include <bit>
#include <cmath>
#include <limits>
#include <utility>
#include "flight/aerodynamic_rotation_internal.h"

namespace tfx::flight {
namespace {
using calculation = double;
double constexpr normalised_word{3.051850947599719e-05}, metres_per_world_unit{2.4384};
double constexpr native_pi{3.14159}, inverse_degrees{.005555555555555555}, units_per_half_turn{32768};
double constexpr feet_per_world_unit{8}, metres_per_foot{.3048}, approach_setback_metres{2500};
double constexpr approach_degrees{7}, kilometres_per_metre{.001}, metres_per_kilometre{1000};
double constexpr initial_body_speed{140}, stopped_body_speed{.5}, resumed_seconds{.05}, gravity{9.81}, suspension_points{3};
double constexpr glide_angle{native_pi * approach_degrees * inverse_degrees};
float constexpr metres_per_fixed_speed{.4762499928474426f};
int32_t constexpr cell_width{2048}, cell_centre{1024}, landing_score_change{-10000};

int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
int32_t subtract(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
calculation normalised(int32_t const value) { return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(value))) * normalised_word; }
void copy_basis(aerodynamic_basis &basis, aerodynamic_frame const &frame) {
  for(unsigned int axis{0}; axis != 3; ++axis) {
    basis.forward[axis] = static_cast<double>(normalised(frame.forward[axis]));
    basis.right[axis] = static_cast<double>(normalised(frame.right[axis]));
    basis.down[axis] = static_cast<double>(normalised(frame.down[axis]));
  }
}
void toggle_gear(aerodynamic_flight &flight, damage_state &damage, automatic_landing_context const &context, automatic_landing_events &events) {
  // Native 0x84990 has no landed guard; setup can raise deployed gear and the
  // common tail lowers it again, preserving both ordered model/sound requests.
  auto &condition{damage.systems[std::to_underlying(aircraft_system::landing_gear)]};
  if(context.inhibited || flight.response.motion.collision || context.ejected || condition == system_condition::failed) return;
  if(flight.gear_fraction == 0) {
    flight.gear_fraction = 1;
    condition = system_condition::active;
    events.ordered.push_back(automatic_landing_event::gear_lowered);
  } else {
    flight.gear_fraction = 0;
    condition = system_condition::normal;
    events.ordered.push_back(automatic_landing_event::gear_raised);
  }
}
void prepare(automatic_landing &landing, aerodynamic_flight &flight, damage_state &damage, automatic_landing_context const &context, automatic_landing_events &events) {
  if(flight.powerplant.stop_afterburner()) events.ordered.push_back(automatic_landing_event::afterburner_stopped);
  toggle_gear(flight,damage,context,events);
  auto &motion{flight.response.motion};
  auto &data{flight.data};
  motion.pose.frame = {};
  landing.approach.pitch_degrees = -approach_degrees;
  rotate_aerodynamic_units(motion,aerodynamic_rotation::yaw,static_cast<double>(math::signed_word(context.runway.port.heading) + units_per_half_turn));
  copy_basis(data.basis,motion.pose.frame);
  auto &approach{landing.approach};
  auto const &runway{context.runway};
  approach.destination_metres = {
    truncate(static_cast<calculation>(runway.cell.x * cell_width + cell_centre + runway.port.offset.x) * feet_per_world_unit * metres_per_foot),
    0,
    truncate(static_cast<calculation>(runway.cell.y * cell_width + cell_centre + runway.port.offset.y) * feet_per_world_unit * metres_per_foot),
  };
  approach.flaring = false;
  motion.pose.metres_ned.x = static_cast<double>(static_cast<calculation>(approach.destination_metres.z) - static_cast<calculation>(data.basis.forward.x) * approach_setback_metres);
  motion.pose.metres_ned.y = static_cast<double>(static_cast<calculation>(approach.destination_metres.x) - static_cast<calculation>(data.basis.forward.y) * approach_setback_metres);
  calculation const north{static_cast<calculation>(subtract(approach.destination_metres.z,approach.player_metres.z))};
  calculation const east{static_cast<calculation>(subtract(approach.destination_metres.x,approach.player_metres.x))};
  double const squared{static_cast<double>(north * north + east * east)};
  approach.distance_km = static_cast<double>(std::sqrt(static_cast<calculation>(squared)) * kilometres_per_metre);
  auto const sine{std::sin(glide_angle)};
  calculation const deck{static_cast<calculation>(runway.carrier ? 20 : 0) * metres_per_foot * feet_per_world_unit};
  motion.pose.metres_ned.z = static_cast<double>(static_cast<calculation>(approach.distance_km) * -metres_per_kilometre * sine - deck);
  data.metres_per_second = flight.aircraft.low_speed_reference;
  motion.velocity_ned = {static_cast<double>(static_cast<calculation>(data.basis.forward.x) * initial_body_speed),static_cast<double>(static_cast<calculation>(data.basis.forward.y) * initial_body_speed),static_cast<double>(sine * initial_body_speed)};
  motion.ground_velocity_body = motion.air_velocity_body = vec3d{initial_body_speed,0,0};
  motion.angular_rate = vec3d{0,0,0};
  flight.response.sideslip_radians = data.angle_of_attack = 0;
  data.fixed_speed = 0;
  flight.view.position.y = 0;
  events.score_change = landing_score_change;
  events.clear_basic_speed = true;
}
void finish(automatic_landing &landing, aerodynamic_flight &flight, automatic_landing_context const &context, automatic_landing_events &events) {
  events.ordered.push_back(automatic_landing_event::stopped);
  auto &motion{flight.response.motion};
  flight.powerplant.engine_factor = vec2d{0,0};
  motion.velocity_ned = motion.ground_velocity_body = motion.air_velocity_body = vec3d{stopped_body_speed,0,0};
  flight.ground.ground.landed = true;
  // Native 0x95944 rebuilds from the PREVIOUS view, using the same cyclic
  // cockpit-offset component order as 0x954fc. Its down result is immediately
  // overwritten by the suspension-height calculation below.
  auto const &body{flight.view.orientation};
  for(unsigned int axis{0}; axis != 2; ++axis) {
    double const forward{static_cast<double>(normalised(body.forward[axis]) * flight.aircraft.viewpoint.forward)};
    calculation const offset{static_cast<calculation>(forward) - normalised(body.down[axis]) * flight.aircraft.viewpoint.upward};
    motion.pose.metres_ned[axis] = static_cast<double>((static_cast<calculation>(flight.view.position[(axis + 2) % 3]) - offset) * metres_per_world_unit);
  }
  calculation const ground{static_cast<calculation>(context.ground_elevation_steps) * metres_per_foot * feet_per_world_unit};
  calculation const down{ground - flight.aircraft.ground.offsets[1].z};
  motion.pose.metres_ned.z = static_cast<double>(static_cast<calculation>(flight.data.mass_kg) * gravity / (static_cast<calculation>(flight.aircraft.ground.contact.spring_stiffness) * suspension_points) + down);
  flight.data.metres_per_second = flight.data.altitude_km = 0;
  flight.seconds = resumed_seconds;
  landing.phase = automatic_landing_phase::inactive;
  flight.response.low_speed_ground_contact = true;
  rotate_aerodynamic_units(motion,aerodynamic_rotation::pitch,static_cast<double>(static_cast<calculation>(context.resting_pitch_radians) * units_per_half_turn / native_pi));
}
} // anonymous namespace

automatic_landing_events advance_automatic_landing(automatic_landing &landing, aerodynamic_flight &flight, damage_state &damage, automatic_landing_context const &context) {
  automatic_landing_events events;
  if(landing.phase == automatic_landing_phase::inactive) return events;
  if(landing.phase == automatic_landing_phase::prepare) {
    prepare(landing,flight,damage,context,events);
    landing.phase = automatic_landing_phase::approach;
  }
  if(advance_automatic_approach(landing.approach,flight.response.motion,flight.data.basis,flight.data.metres_per_second,{
    .runway_heading{context.runway.port.heading},
    .seconds{flight.seconds},
    .left_main_wheel_down{flight.aircraft.ground.offsets[1].z},
    .carrier{context.runway.carrier},
  })) finish(landing,flight,context,events);
  if(flight.gear_fraction == 0) toggle_gear(flight,damage,context,events);
  flight.data.fixed_speed = truncate(static_cast<calculation>(flight.data.metres_per_second) / metres_per_fixed_speed);
  flight.view = update_aerodynamic_view(flight.response.motion.pose,flight.aircraft.stores_and_mounts.stores.aircraft,flight.aircraft.viewpoint);
  flight.response.motion.integer_world_y = flight.view.integer_position.y;
  // Preserve original shared scratch at the callback handover. Ordinary route
  // guidance rewrites these positions in world units; landing stores metres.
  flight.autopilot.navigation.target_position = vec3<int32_t>{landing.approach.destination_metres};
  flight.autopilot.navigation.player_position = vec3<int32_t>{landing.approach.player_metres};
  flight.autopilot.navigation.route_distance = landing.approach.distance_km;
  flight.autopilot.guidance.heading_error = landing.approach.pitch_degrees;
  return events;
}

} // namespace tfx::flight
