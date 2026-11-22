#pragma once

#include "aerodynamic_snapshot.h"

inline std::vector<uint64_t> aerodynamic_autopilot_snapshot(tfx::flight::aerodynamic_autopilot const &s) {
  std::vector<uint64_t> values;
  auto const &g{s.guidance};
  for(auto const value : {g.heading_error, g.ground_speed, g.climb_speed_limit, g.requested_vertical_speed, g.desired_flight_path, g.actual_flight_path, g.maximum_bank, g.desired_bank, g.roll_angle, g.roll_error, g.roll_control_rate, g.commanded_roll_rate, g.saved_roll_rate, g.yaw_demand, s.navigation.route_distance, s.navigation.followed_speed_radians}) values.push_back(std::bit_cast<uint64_t>(value));
  for(auto const *vector : {&s.navigation.target_position, &s.navigation.player_position}) {
    for(auto const value : *vector) values.push_back(static_cast<uint32_t>(value));
  }
  values.push_back(static_cast<uint16_t>(s.navigation.route_index));
  values.push_back(s.navigation.target_pitch);
  auto const &r{s.recovery};
  for(auto const value : {static_cast<unsigned int>(r.configuration), static_cast<unsigned int>(r.saved_configuration), static_cast<unsigned int>(r.saved_mode), static_cast<unsigned int>(r.saved_enabled), static_cast<unsigned int>(r.saved_air_brake)}) values.push_back(value);
  values.push_back(std::bit_cast<uint64_t>(r.saved_throttle));
  values.push_back(r.saved_afterburner);
  values.push_back(r.model_reset_requested);
  return values;
}
