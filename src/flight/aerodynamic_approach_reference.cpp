#include "aerodynamic_approach_reference.h"
#include <cmath>

namespace tfx::flight {
namespace {
using calculation = double;
double constexpr gravity{9.81}, native_pi{3.14159}, inverse_degrees{.005555555555555555};
double constexpr reference_glide_degrees{7}, minimum_path_speed{1}, degrees_per_half_turn{180};
double constexpr reference_glide_angle{native_pi * reference_glide_degrees * inverse_degrees};
} // anonymous namespace

void update_aerodynamic_approach_reference(aerodynamic_approach_reference &state, aerodynamic_approach_reference_context const &context) {
  auto const &data{context.data};
  auto const &geometry{context.geometry};
  calculation const speed{context.low_speed_reference};
  if(context.aircraft != player_aircraft::nighthawk && context.flaps_condition != system_condition::failed) {
    calculation const dynamic{static_cast<calculation>(data.atmosphere.density) * speed * speed * geometry.wing_area};
    state.flap_fraction = static_cast<double>((static_cast<calculation>(data.mass_kg) * 2 * gravity / dynamic - geometry.zero_lift) / (static_cast<calculation>(geometry.flap_lift_gradient) * geometry.maximum_flap_angle));
    if(state.flap_fraction > 1) state.flap_fraction = 1;
    if(state.flap_fraction < 0) state.flap_fraction = 0;
  } else state.flap_fraction = 0;
  calculation const dynamic{std::cos(reference_glide_angle) * data.atmosphere.density * speed * speed * geometry.wing_area};
  state.lift_coefficient = static_cast<double>(static_cast<calculation>(data.mass_kg) * 2 * gravity / dynamic);
  state.angle_of_attack = static_cast<double>((static_cast<calculation>(state.lift_coefficient) - (static_cast<calculation>(geometry.flap_lift_gradient) * geometry.maximum_flap_angle * state.flap_fraction + geometry.zero_lift)) / context.coefficients.lift_slope);
  auto const &velocity{context.motion.velocity_ned};
  state.ground_speed = static_cast<double>(std::sqrt(static_cast<calculation>(velocity.y) * velocity.y + static_cast<calculation>(velocity.x) * velocity.x + static_cast<calculation>(velocity.z) * velocity.z));
  if(state.ground_speed > minimum_path_speed && velocity.z < state.ground_speed && velocity.z > -state.ground_speed) {
    calculation const ratio{static_cast<calculation>(velocity.z) / state.ground_speed};
    state.flight_path_degrees = static_cast<double>(std::atan2(ratio,std::sqrt(1.0 - ratio * ratio)) * degrees_per_half_turn / native_pi);
  }
}

} // namespace tfx::flight
