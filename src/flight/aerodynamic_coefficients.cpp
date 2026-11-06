#include "aerodynamic_coefficients.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr native_pi{3.14159}, half{.5}, one_third{.3333333333333333};
double constexpr nighthawk_lift_slope{1.78};
double constexpr raptor_lift_quadratic{.64}, raptor_lift_base{2.88};
double constexpr raptor_linear_gradient{3.75}, raptor_linear_base{.75};
double constexpr raptor_secondary_mach{1}, raptor_secondary_gradient{.15225};
double constexpr raptor_secondary_width{.07}, raptor_secondary_base{4.5};
double constexpr raptor_high_mach{2.1}, raptor_high_gradient{2.5}, raptor_high_base{2};
double constexpr eurofighter_lift_gradient{.469}, eurofighter_lift_base{2.66};
double constexpr eurofighter_cap_gradient{1.8}, eurofighter_cap_base{5.82};
double constexpr damping_mach_reference{3};

} // anonymous namespace

aerodynamic_coefficients update_aerodynamic_coefficients(player_aircraft const aircraft, double const mach, aerodynamic_coefficient_parameters const &p) {
  calculation const speed{mach};
  aerodynamic_coefficients result{
    .reference_drag{p.base_drag},
  };
  if(speed > p.critical_mach) {
    result.wave_drag = p.wave_drag_increment;
    if(speed < p.full_wave_drag_mach) {
      calculation const phase{(speed - (static_cast<calculation>(p.critical_mach) + p.full_wave_drag_mach) * half) * (static_cast<calculation>(native_pi) / (static_cast<calculation>(p.full_wave_drag_mach) - p.critical_mach))};
      result.wave_drag = static_cast<double>((static_cast<calculation>(p.wave_drag_increment) * half) * (1.0 + std::sin(phase)));
    }
  }
  result.zero_lift_drag = static_cast<double>(static_cast<calculation>(result.reference_drag) + result.wave_drag);
  if(aircraft == player_aircraft::nighthawk) result.lift_slope = nighthawk_lift_slope;
  if(aircraft == player_aircraft::raptor) {
    result.lift_slope = static_cast<double>(speed * raptor_lift_quadratic * speed + raptor_lift_base);
    double const linear_term{static_cast<double>(speed * raptor_linear_gradient)};
    double const linear{static_cast<double>(raptor_linear_base + static_cast<calculation>(linear_term))};
    if(result.lift_slope < linear) {
      double const secondary_mach{static_cast<double>((speed - raptor_secondary_mach) * raptor_secondary_gradient)};
      double const inverse_width{1.0 / raptor_secondary_width};
      double const secondary_term{static_cast<double>(static_cast<calculation>(secondary_mach) * inverse_width)};
      double const secondary{static_cast<double>(raptor_secondary_base + static_cast<calculation>(secondary_term))};
      result.lift_slope = linear;
      if(linear > secondary) {
        double const high_mach{static_cast<double>(speed - raptor_high_mach)};
        double const high_term{static_cast<double>(static_cast<calculation>(high_mach) * raptor_high_gradient * high_mach)};
        double const high{static_cast<double>(raptor_high_base + static_cast<calculation>(high_term))};
        result.lift_slope = secondary;
        if(secondary > high) result.lift_slope = high;
      }
    }
  }
  if(aircraft == player_aircraft::eurofighter) {
    result.lift_slope = static_cast<double>(speed * eurofighter_lift_gradient + eurofighter_lift_base);
    double const cap_term{static_cast<double>(speed * eurofighter_cap_gradient)};
    double const cap{static_cast<double>(eurofighter_cap_base - static_cast<calculation>(cap_term))};
    if(result.lift_slope > cap) result.lift_slope = cap;
  }
  if(aircraft != player_aircraft::nighthawk) result.lift_slope = static_cast<double>((static_cast<calculation>(p.additional_lifting_area) / p.wing_area + 1.0) * result.lift_slope);
  calculation const combined{static_cast<calculation>(result.lift_slope) + result.zero_lift_drag};
  result.pitch_stability = static_cast<double>((static_cast<calculation>(p.pitch_stability_base) + p.pitch_stability_correction) * combined);
  result.pitch_damping = static_cast<double>(((static_cast<calculation>(damping_mach_reference) - speed) * p.pitch_damping_mach_gradient * one_third + p.pitch_damping_base) * combined);
  result.elevator_moment = static_cast<double>(static_cast<calculation>(p.elevator_moment_gradient) * result.lift_slope);
  result.elevator_lift = static_cast<double>(static_cast<calculation>(p.elevator_lift_gradient) * result.lift_slope);
  result.roll_damping = static_cast<double>(static_cast<calculation>(p.roll_damping_gradient) * result.lift_slope);
  result.aileron_effectiveness = static_cast<double>(static_cast<calculation>(result.lift_slope) * p.aileron_gradient);
  return result;
}

} // namespace tfx::flight
