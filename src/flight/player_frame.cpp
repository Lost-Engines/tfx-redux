#include "player_frame.h"
#include <cmath>
#include <limits>
#include <utility>

namespace tfx::flight {

int32_t finish_player_frame(aerodynamic_flight &flight, damage_state &damage, fuel_gauge &fuel, player_frame_context const &context) {
  using calculation = double;
  flight.autopilot.recovery.model_reset_requested = false;
  if((flight.ground.ground.landed && flight.view.orientation.down.y < 0)
    || (context.terrain_present && !(context.ground_elevation_steps >= flight.view.position.y))) {
    flight.response.motion.collision = 2;
    flight.view.position.y = context.ground_elevation_steps;
  }
  // This is an indicator refresh, even after an intermittent engine fault.
  // The accumulated loss of power remains in each engine's factor.
  for(unsigned int engine{0}; engine != 2; ++engine) {
    auto const condition{flight.powerplant.engine_factor[engine] != 0 ? system_condition::active : system_condition::normal};
    flight.powerplant.engine_condition[engine] = condition;
    damage.systems[std::to_underlying(engine ? aircraft_system::right_engine : aircraft_system::left_engine)] = condition;
  }
  fuel.sample(flight.powerplant.fuel_readout);
  flight.powerplant.fuel_readout = fuel.reading;
  vec3f delta;
  for(unsigned int axis{0}; axis != 3; ++axis) delta[axis] = static_cast<float>(static_cast<calculation>(context.previous_position[axis]) - flight.view.position[axis]);
  calculation const x{delta.x}, y{delta.y}, z{delta.z};
  double constexpr movement_fraction{256};
  calculation const measured{std::sqrt((x * x + y * y) + z * z) / context.elapsed * movement_fraction};
  if(!std::isfinite(measured) || measured < std::numeric_limits<int32_t>::min() || measured >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(measured);
}

} // namespace tfx::flight
