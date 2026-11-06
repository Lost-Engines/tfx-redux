#include "aerodynamic.h"
#include <utility>

namespace tfx::flight {
namespace {

double constexpr initial_autopilot_speed{180.25};
float constexpr carrier_placeholder_height{5}, carrier_view_height{-100};
int32_t constexpr carrier_integer_height{-100};
int16_t constexpr carrier_height_limit{-10};

} // anonymous namespace

auto aerodynamic_flight::configure_controls(input_configuration const configuration)->void {
  bool const combat{configuration == input_configuration::combat};
  bool const realistic{model == aerodynamic_model::realistic};
  autopilot.recovery.configuration = std::to_underlying(configuration);
  input_gains = {
    .pitch{combat ? realistic ? .8 : 1.0 : realistic ? .12 : .15},
    .roll{combat ? realistic ? .88 : 1.1 : realistic ? .152 : .19},
    .rudder{realistic ? .024 : .03},
    .joystick{combat || aircraft.stores_and_mounts.stores.aircraft == player_aircraft::nighthawk ? 1.0 : .5},
  };
}

void aerodynamic_flight::reset_motion(int16_t const ground_elevation_steps, bool const terrain_present) {
  auto &motion{response.motion};
  auto &ground{this->ground.ground};
  aerodynamic_start_state start{
    .motion{motion},
    .stores{stores},
    .engine_factor{vec2d{powerplant.engine_factor}},
    .fuel_kg{powerplant.fuel},
    .mass_kg{data.mass_kg},
    .seconds{seconds},
    .g_load{response.g_load},
    .track_heading{data.track_heading},
    .ground_steering{ground.ground_steering},
  };
  initialise_aerodynamic_motion(start,aircraft,mounted_stores,{
    .orientation{view.orientation},
    .position{vec3f{view.position}},
    .fixed_speed{data.fixed_speed},
    .heading{view.angles.y},
    .ground_elevation_steps{ground_elevation_steps},
    .landed{ground.landed},
    .cold_start{cold_start},
    .terrain_present{terrain_present},
  });
  motion = start.motion;
  stores = start.stores;
  powerplant.engine_factor = start.engine_factor;
  powerplant.fuel = start.fuel_kg;
  data.mass_kg = start.mass_kg;
  seconds = start.seconds;
  response.g_load = start.g_load;
  data.track_heading = start.track_heading;
  ground.ground_steering = start.ground_steering;
  coefficient_parameters.wing_area = aircraft.surfaces.wing_area;
}

aerodynamic_flight initialise_aerodynamic_flight(aerodynamic_airframe const &aircraft, loadout const &stores, aerodynamic_initial_context const &initial, aerodynamic_wind_parameters const &wind_parameters, damage_state &damage, math::random_state &random) {
  auto const &context{initial.placement};
  aerodynamic_flight state;
  state.aircraft = aircraft;
  state.model = initial.model;
  state.mounted_stores = stores;
  state.cold_start = context.cold_start;
  if(initial.model == aerodynamic_model::realistic) state.input_gains = {
    .pitch{.4},
    .roll{.152},
    .rudder{.024},
    .joystick{1},
  };
  state.coefficients = aircraft.starting_coefficients;
  state.gear_fraction = damage.condition(aircraft_system::landing_gear) == system_condition::active ? 1 : 0;
  state.manual.controls.autopilot.metres_per_second = initial_autopilot_speed;
  state.data.basis = {
    .forward{0,0,0},
    .right{0,0,0},
    .down{0,0,0},
  };
  damage.accumulated = 0;
  initialise_aerodynamic_wind(state.wind,wind_parameters,random,initial.wind,false);
  state.stores = accumulate_stores(state.aircraft.stores_and_mounts.stores,stores);
  state.powerplant.fuel_capacity = state.aircraft.stores_and_mounts.stores.fuel_capacity;
  state.powerplant.extra_fuel_capacity = state.aircraft.stores_and_mounts.stores.aircraft == player_aircraft::eurofighter ? state.powerplant.fuel_capacity : 0;
  state.powerplant.fuel = state.stores.fuel_capacity;
  aerodynamic_start_state start{
    .stores{state.stores},
    .engine_factor{1,1},
    .fuel_kg{state.powerplant.fuel},
  };
  initialise_aerodynamic_motion(start,state.aircraft,stores,context);
  state.stores = start.stores;
  state.seconds = start.seconds;
  state.response.motion = start.motion;
  state.response.motion.air_velocity_body.x = start.motion.ground_velocity_body.x;
  state.response.motion.integer_world_y = static_cast<int32_t>(context.position.y);
  state.powerplant.engine_factor = start.engine_factor;
  state.powerplant.fuel = start.fuel_kg;
  state.data.mass_kg = start.mass_kg;
  state.data.fixed_speed = context.fixed_speed;
  state.data.atmosphere.sea_level_kelvin = initial.surface_celsius ? static_cast<double>(static_cast<double>(*initial.surface_celsius) + 273) : 288.15;
  state.data.track_heading = start.track_heading;
  state.ground.ground.ground_steering = start.ground_steering;
  state.ground.ground.landed = context.landed;
  state.coefficient_parameters = state.aircraft.initial_coefficients();
  // Native applies this setup again after physical initialization/calibration.
  if(context.cold_start) {
    state.powerplant.engine_factor = vec2d{0,0};
    damage.systems[std::to_underlying(aircraft_system::left_engine)] = system_condition::normal;
    damage.systems[std::to_underlying(aircraft_system::right_engine)] = system_condition::normal;
    if(aircraft.stores_and_mounts.stores.aircraft == player_aircraft::raptor && context.terrain_present && context.ground_elevation_steps < carrier_height_limit) {
      state.aircraft.use_carrier_setup();
      state.coefficient_parameters.wing_area = state.aircraft.surfaces.wing_area;
    }
  }
  state.view.orientation = context.orientation;
  state.view.position = context.position;
  for(unsigned int axis{0}; axis != 3; ++axis) state.view.integer_position[axis] = static_cast<int32_t>(context.position[axis]);
  state.view.angles = context.orientation.angles();
  if(context.position.y == carrier_placeholder_height) {
    state.aircraft.use_carrier_setup();
    state.coefficient_parameters.wing_area = state.aircraft.surfaces.wing_area;
    state.view.position.y = carrier_view_height;
    state.view.integer_position.y = state.response.motion.integer_world_y = carrier_integer_height;
  }
  state.powerplant.engine_condition = {
    damage.condition(aircraft_system::left_engine),
    damage.condition(aircraft_system::right_engine),
  };
  state.manual.air_brake_condition = damage.condition(aircraft_system::air_brake);
  return state;
}

} // namespace tfx::flight
