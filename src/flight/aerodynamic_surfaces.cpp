#include "aerodynamic_surfaces.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr half{.5}, chute_drag_coefficient{1.2};

} // anonymous namespace

void update_aerodynamic_surfaces(aerodynamic_surface_state &state, aerodynamic_surface_parameters const &p, aerodynamic_surface_context const &context) {
  auto const &c{context.coefficients};
  calculation const alpha{context.angle_of_attack}, speed{context.metres_per_second};
  state.flap_lift_offset = static_cast<double>(static_cast<calculation>(p.flap_lift_gradient) * context.flap_fraction * p.maximum_flap_angle + p.zero_lift);
  state.positive_lift_limit = static_cast<double>(static_cast<calculation>(p.flap_limit_gradient) * context.flap_fraction * p.maximum_flap_angle + p.maximum_lift);
  state.negative_lift_limit = -state.positive_lift_limit;
  state.wing_lift_coefficient = static_cast<double>(static_cast<calculation>(c.lift_slope) * alpha + state.flap_lift_offset);
  state.stalled = context.aircraft == player_aircraft::nighthawk && alpha > p.critical_angle;
  if(state.wing_lift_coefficient > state.positive_lift_limit) {
    if(context.aircraft == player_aircraft::nighthawk) state.wing_lift_coefficient = state.positive_lift_limit;
    else {
      double const span{static_cast<double>(static_cast<calculation>(p.automatic_flap_gradient) * p.maximum_automatic_flap_angle)};
      state.automatic_flap_fraction = static_cast<double>((static_cast<calculation>(state.wing_lift_coefficient) - state.positive_lift_limit) / span);
      if(state.automatic_flap_fraction > 1) {
        state.automatic_flap_fraction = 1;
        state.wing_lift_coefficient = static_cast<double>(static_cast<calculation>(state.positive_lift_limit) + span);
        state.stalled = true;
      }
      if(state.automatic_flap_fraction != 0 && context.flaps_condition == system_condition::failed) {
        state.automatic_flap_fraction = 0;
        state.wing_lift_coefficient = state.positive_lift_limit;
        state.stalled = true;
      }
    }
  }
  if(state.wing_lift_coefficient < state.negative_lift_limit) state.wing_lift_coefficient = state.negative_lift_limit;
  state.effective_angle = static_cast<double>(static_cast<calculation>(c.elevator_lift) * context.elevator * p.maximum_elevator_angle + alpha);
  state.lift_coefficient = static_cast<double>(static_cast<calculation>(state.wing_lift_coefficient) + state.effective_angle);
  state.lift = static_cast<double>(static_cast<calculation>(context.dynamic_pressure) * p.wing_area * state.lift_coefficient);
  calculation const induced{static_cast<calculation>(state.lift_coefficient) * p.induced_drag * state.lift_coefficient};
  calculation const brake{static_cast<calculation>(p.air_brake_drag) * context.air_brake_fraction};
  calculation const gear{static_cast<calculation>(p.gear_drag) * context.gear_fraction};
  calculation const elevator{static_cast<calculation>(p.elevator_drag) * context.elevator * p.maximum_elevator_angle * context.elevator * p.maximum_elevator_angle};
  state.drag_coefficient = static_cast<double>(induced + c.zero_lift_drag + brake + gear + elevator);
  state.drag = static_cast<double>((static_cast<calculation>(p.chute_area) * chute_drag_coefficient * static_cast<int>(context.chute_deployed) + static_cast<calculation>(p.wing_area) * state.drag_coefficient) * context.dynamic_pressure);
  if(context.calculate_moments) {
    calculation pitch{alpha * c.pitch_stability * speed + static_cast<calculation>(p.zero_pitch_moment) * speed};
    pitch += static_cast<calculation>(c.elevator_moment) * context.elevator * p.maximum_elevator_angle * speed;
    pitch += static_cast<calculation>(c.pitch_damping) * context.pitch_rate * p.mean_chord * half;
    state.pitch_rate_moment_term = static_cast<double>(pitch);
    auto const sine{std::sin(alpha)}, cosine{std::cos(alpha)};
    calculation const chute{static_cast<calculation>(p.chute_pitch_arm) * sine * context.dynamic_pressure * chute_drag_coefficient * p.chute_area * static_cast<int>(context.chute_deployed)};
    calculation const main_moment{speed * (static_cast<calculation>(context.density) * half) * p.wing_area * p.mean_chord * state.pitch_rate_moment_term};
    calculation const brake_moment{static_cast<calculation>(context.dynamic_pressure) * (static_cast<calculation>(p.air_brake_moment_arm.x) * sine - static_cast<calculation>(p.air_brake_moment_arm.y) * cosine) * p.wing_area * p.air_brake_drag * context.air_brake_fraction};
    state.pitch_moment = static_cast<double>(chute + main_moment + brake_moment);
    calculation const control{static_cast<calculation>(c.aileron_effectiveness) * context.aileron * p.maximum_aileron_angle + static_cast<calculation>(c.roll_sideslip) * context.sideslip};
    state.roll_rate_moment_term = static_cast<double>(control * speed + static_cast<calculation>(c.roll_damping) * context.roll_rate * p.wing_span * half);
    state.roll_moment = static_cast<double>(static_cast<calculation>(context.density) * half * speed * p.wing_area * p.wing_span * state.roll_rate_moment_term);
  }
  auto const cosine{std::cos(alpha)};
  double const sine{static_cast<double>(std::sin(alpha))};
  state.force.x = static_cast<double>(-(static_cast<calculation>(state.drag) * cosine - static_cast<calculation>(state.lift) * sine));
  state.force.z = static_cast<double>(-static_cast<calculation>(state.lift) * cosine - static_cast<calculation>(state.drag) * sine);
  state.moment.x = state.roll_moment;
  state.moment.y = state.pitch_moment;
}

} // namespace tfx::flight
