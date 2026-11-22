#pragma once

#include "flight/aerodynamic_coefficients.h"
#include "flight/aerodynamic_ground.h"
#include "flight/aerodynamic_pose.h"
#include "flight/aerodynamic_powerplant.h"
#include "flight/aerodynamic_surfaces.h"
#include "flight/airframes.h"
#include "flight/atmosphere.h"

namespace tfx::flight {

struct aerodynamic_carrier_setup {
  double maximum_sink_speed{0};
  double wing_area{0};
  double air_brake_drag{0};
};
struct aerodynamic_airframe {
  airframe stores_and_mounts{};
  aerodynamic_coefficient_parameters coefficient_base{};
  aerodynamic_coefficients starting_coefficients{};
  aerodynamic_powerplant_parameters powerplant{};
  aerodynamic_surface_parameters surfaces{};
  aerodynamic_viewpoint viewpoint{};
  aerodynamic_ground_parameters ground{};
  aerodynamic_carrier_setup carrier_setup{};
  atmosphere_parameters atmosphere{};
  double rudder_effectiveness{0};
  double maximum_autopilot_speed{0};
  double empty_mass_kg{0};
  double maximum_g{0};
  double minimum_g{0};
  double cockpit_upward_metres{0};
  double stall_metres_per_second{0};
  double low_speed_reference{0};
  double cruise_mach{0};
  void use_carrier_setup();
  aerodynamic_coefficient_parameters initial_coefficients(double previous_flap_lift_offset = 0) const;
};
std::array<aerodynamic_airframe,3> load_aerodynamic_airframes(std::span<uint8_t const> executable);

} // namespace tfx::flight
