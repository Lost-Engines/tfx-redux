#include "aerodynamic_navigation.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tfx::flight {
namespace {
using calculation = double;
double constexpr world_units_per_metre{.4101049868766404}, metres_per_foot{.3048}, feet_per_world_unit{8};
double constexpr route_distance_scale{.001}, waypoint_advance_distance{4}, untracked_distance{1000};
double constexpr speed_metres_per_second{.515}, native_pi{3.14159}, inverse_degrees{.005555555555555556}, inverse_angle_half_turn{3.0517578125e-05};
int32_t constexpr map_cell_width{2048}, map_cell_centre{1024};
uint16_t constexpr route_mode{0}, heading_mode{1}, follow_mode{2};
int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void measure_route_distance(aerodynamic_navigation &state) {
  calculation const x{static_cast<calculation>(state.target_position.x) - state.player_position.x};
  calculation const z{static_cast<calculation>(state.target_position.z) - state.player_position.z};
  double const squared{static_cast<double>(z * z + x * x)};
  state.route_distance = static_cast<double>(std::sqrt(static_cast<calculation>(squared)) * route_distance_scale);
}
void update_bearing(aerodynamic_navigation &state, aerodynamic_autopilot_targets &targets) {
  auto const bearing{math::bearing(state.player_position,state.target_position)};
  state.target_pitch = bearing.x;
  targets.heading = bearing.y;
}
} // anonymous namespace

bool select_player_route(aerodynamic_navigation &state, std::span<vec2<uint8_t> const> const cells, player_route_action const action) {
  if(cells.size() > static_cast<unsigned int>(std::numeric_limits<int16_t>::max())) throw std::invalid_argument{"player route exceeds the original signed-word count"};
  int16_t index{state.route_index};
  switch(action) {
    case player_route_action::previous: index = math::signed_word(static_cast<uint16_t>(index - 1)); break;
    case player_route_action::next: index = math::signed_word(static_cast<uint16_t>(index + 1)); break;
    case player_route_action::first: index = 1; break;
    case player_route_action::last: index = static_cast<int16_t>(cells.size()); break;
    case player_route_action::current: break;
  }
  if(index < 1 || static_cast<unsigned int>(index) > cells.size()) return false;
  auto const &cell{cells[static_cast<unsigned int>(index - 1)]};
  state.route_index = index;
  state.target_position.x = cell.x * map_cell_width + map_cell_centre;
  state.target_position.z = cell.y * map_cell_width + map_cell_centre;
  return true;
}

void update_aerodynamic_navigation(aerodynamic_navigation &state, aerodynamic_guidance &guidance, aerodynamic_autopilot_targets &targets, aerodynamic_motion const &motion, aerodynamic_navigation_context const &context) {
  state.player_position = {
    truncate(static_cast<calculation>(motion.pose.metres_ned.y) * world_units_per_metre),
    truncate(static_cast<calculation>(motion.pose.metres_ned.z) * world_units_per_metre),
    truncate(static_cast<calculation>(motion.pose.metres_ned.x) * world_units_per_metre),
  };
  guidance.desired_flight_path = 0;
  select_player_route(state,context.route_cells,player_route_action::current);
  guidance.saved_roll_rate = motion.angular_rate.x;
  if(targets.mode == follow_mode && context.followed_aircraft) {
    auto const &followed{*context.followed_aircraft};
    for(unsigned int axis{0}; axis != 3; ++axis) state.target_position[axis] = followed.position_q8[axis] >> 8;
    update_bearing(state,targets);
    targets.down_metres = static_cast<double>(static_cast<calculation>(state.target_position.y) * metres_per_foot * feet_per_world_unit);
    measure_route_distance(state);
    int16_t const fixed_speed{math::signed_word(static_cast<uint16_t>(followed.speed_q16 >> 16))};
    guidance.ground_speed = static_cast<double>(static_cast<calculation>(fixed_speed) * speed_metres_per_second);
    state.followed_speed_radians = static_cast<double>(static_cast<calculation>(fixed_speed) * inverse_degrees * native_pi);
    guidance.desired_flight_path = static_cast<double>(-static_cast<calculation>(native_pi) * inverse_angle_half_turn * math::signed_word(state.target_pitch));
  }
  if(targets.mode == route_mode) {
    measure_route_distance(state);
    if(state.route_distance < waypoint_advance_distance && (select_player_route(state,context.route_cells,player_route_action::next) || select_player_route(state,context.route_cells,player_route_action::first))) measure_route_distance(state);
    update_bearing(state,targets);
  }
  if(targets.mode == heading_mode || (targets.mode == follow_mode && !context.followed_aircraft)) state.route_distance = untracked_distance;
}

} // namespace tfx::flight
