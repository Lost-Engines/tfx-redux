#include "impact_prediction.h"
#include <stdexcept>

namespace tfx::weapons {

vec3<int32_t> predict_bomb_impact(definition const &weapon, uint8_t const index, vec3<int32_t> const &anchor_q8,
  vec3<math::angle> const &angles, int32_t const speed, vec2<int32_t> const &height_bounds) {
  if(weapon.category != bomb_impact_category) throw std::invalid_argument{"impact prediction requires a category-9 bomb"};
  return predict_bomb_impact(weapon, launch(weapon, index, anchor_q8, angles, speed), anchor_q8, height_bounds);
}

vec3<int32_t> predict_bomb_impact(definition const &weapon, projectile shot, vec3<int32_t> const &anchor_q8, vec2<int32_t> const &height_bounds) {
  if(weapon.category != bomb_impact_category) throw std::invalid_argument{"impact prediction requires a category-9 bomb"};
  uint16_t constexpr prediction_step_ticks{10};
  motion_context const context{
    .elapsed{prediction_step_ticks},
    .target{0, 0, 0},
    .player_owned{true},
    .launch_anchor{vec3<int32_t>{anchor_q8}},
    .height_bounds{vec2<int32_t>{height_bounds}},
  };
  math::random_state random;
  do {
    advance_motion(shot, weapon, context, random);
  } while(shot.motion != flight_stage::finished && shot.position.y < 0);
  return vec3<int32_t>{shot.position};
}

} // namespace tfx::weapons
