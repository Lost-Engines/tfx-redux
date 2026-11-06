#include "aerodynamic_launch.h"
#include <cmath>
#include <limits>
#include "flight/basic_parameters.h"
#include "world/environment.h"

namespace tfx::flight {
namespace {
using calculation = double;
float constexpr normalised_word{3.0518509447574615e-05f}, runway_start_setback{120}, airborne_throttle{.85f};
vec2f constexpr carrier_start_offset{2,100};
int16_t constexpr carrier_height_limit{-10};
int32_t constexpr airborne_fixed_speed{250}, carrier_placeholder_height{5};
int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
int32_t centre(int32_t const value) { return value / world::cell_units * world::cell_units + world::cell_units / 2; }
} // anonymous namespace

aerodynamic_flight launch_aerodynamic_flight(aerodynamic_airframe const &aircraft, loadout const &stores, aerodynamic_launch_context const &context, aerodynamic_wind_parameters const &wind_parameters, damage_state &damage, math::random_state &random) {
  vec3<int32_t> const centred{centre(context.position.x),context.position.y,centre(context.position.z)};
  math::attitude orientation;
  orientation.rotate(math::rotation_axis::pitch,context.angles.x);
  orientation.rotate(math::rotation_axis::heading,context.angles.y);
  orientation.rotate(math::rotation_axis::roll,context.angles.z);
  auto state{initialise_aerodynamic_flight(aircraft,stores,{
    .placement{
      .orientation{orientation},
      .position{vec3f{centred}},
      .heading{context.angles.y},
      .landed{context.landed},
      .cold_start{context.landed},
    },
    .model{context.model},
    .wind{context.wind},
    .surface_celsius{context.surface_celsius},
  },wind_parameters,damage,random)};
  // Setup retains authored input angles and integer coordinates separately
  // from its quantized matrix and float stores until the first view export.
  state.view.angles = vec3<math::angle>{context.angles};
  state.view.integer_position.x = centred.x;
  state.view.integer_position.z = centred.z;
  if(centred.y != carrier_placeholder_height) state.view.integer_position.y = centred.y; // Original carrier-placeholder conversion already handled.
  state.response.low_speed_ground_contact = context.landed;
  state.autopilot.navigation.route_index = context.player_route_points >= 2 ? 2 : 1;
  state.autopilot.recovery.saved_air_brake = system_condition::unavailable;
  state.autopilot.saved_air_brake = system_condition::unavailable;
  state.data.fixed_speed = context.landed ? 0 : airborne_fixed_speed;
  state.powerplant.engine_factor = context.landed ? vec2d{0,0} : vec2d{1,1};
  state.powerplant.throttle = context.landed ? 0 : airborne_throttle;
  state.autopilot.recovery.model_reset_requested = true;
  return state;
}

void place_aerodynamic_start(aerodynamic_flight &state, int16_t const ground_elevation_steps) {
  if(!state.cold_start) return;
  state.view.position.y = static_cast<float>(static_cast<calculation>(ground_elevation_steps) - basic_parameters(state.aircraft.stores_and_mounts.stores.aircraft).gear_height);
  bool const advanced_setting{state.model == aerodynamic_model::military_spec || state.model == aerodynamic_model::realistic};
  if(ground_elevation_steps < carrier_height_limit) {
    if(advanced_setting) {
      state.view.position.x = static_cast<float>(static_cast<calculation>(state.view.position.x) + carrier_start_offset.x);
      state.view.position.z = static_cast<float>(static_cast<calculation>(state.view.position.z) + carrier_start_offset.y);
      state.view.integer_position.x += static_cast<int32_t>(carrier_start_offset.x);
      state.view.integer_position.z += static_cast<int32_t>(carrier_start_offset.y);
    }
  } else {
    for(unsigned int axis : {0u,2u}) {
      calculation const setback{static_cast<calculation>(math::signed_word(static_cast<uint16_t>(state.view.orientation.forward[axis]))) * normalised_word * runway_start_setback};
      state.view.integer_position[axis] = truncate(static_cast<calculation>(state.view.integer_position[axis]) - setback);
      state.view.position[axis] = static_cast<float>(static_cast<calculation>(state.view.position[axis]) - setback);
    }
  }
  if(ground_elevation_steps >= carrier_height_limit || advanced_setting) state.reset_motion(ground_elevation_steps,true);
  state.cold_start = false;
}

} // namespace tfx::flight
