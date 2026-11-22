#pragma once

#include <optional>
#include <span>
#include "flight/aerodynamic_guidance.h"

namespace tfx::flight {

enum class player_route_action : int16_t { previous = -1, current, next, first, last };

struct aerodynamic_follow_target {
  vec3<int32_t> position_q8{0,0,0};
  int32_t speed_q16{0};
};
struct aerodynamic_navigation {
  // East/Down/North: world units during route guidance; native automatic
  // landing reuses these retained coordinates in metres at its handover.
  vec3<int32_t> player_position{0,0,0};
  vec3<int32_t> target_position{0,0,0};
  int16_t route_index{1};                                                      // Original one-based selection; invalid requests retain it.
  math::angle target_pitch{0};
  double route_distance{0};                                                    // Thousands of world units, not kilometres.
  double followed_speed_radians{0};                                            // Original retained speed/degree scratch.
};
struct aerodynamic_navigation_context {
  std::span<vec2<uint8_t> const> route_cells{};
  std::optional<aerodynamic_follow_target> followed_aircraft{};
};

// Native 0x751a4: select a packed-map-cell centre without changing target height.
bool select_player_route(aerodynamic_navigation &state, std::span<vec2<uint8_t> const> cells, player_route_action action);

// Native 0x9adae..0x9b003: route advancement or selected-aircraft following,
// followed by the bearing/height inputs consumed by aerodynamic guidance.
void update_aerodynamic_navigation(aerodynamic_navigation &state, aerodynamic_guidance &guidance, aerodynamic_autopilot_targets &targets, aerodynamic_motion const &motion, aerodynamic_navigation_context const &context);

} // namespace tfx::flight
