#include "cannon_prediction.h"

namespace tfx::weapons {

std::array<vec3<int32_t>,5> predict_cannon(definition const &cannon, vec3<int32_t> const &anchor_q8,
  vec3<math::angle> const &angles, int32_t const speed, vec2<int32_t> const &height_bounds) {
  return predict_cannon(cannon,launch(cannon,0,anchor_q8,angles,speed),height_bounds);
}

std::array<vec3<int32_t>,5> predict_cannon(definition const &cannon, projectile shot, vec2<int32_t> const &height_bounds) {
  math::random_state random;
  uint16_t constexpr prediction_step_ticks{20};
  motion_context const context{
    .elapsed{prediction_step_ticks},
    .target{0,0,0},
    .player_owned{true},
    .height_bounds{vec2<int32_t>{height_bounds}},
  };
  auto const advance{[&]{
    if(shot.motion == flight_stage::finished || shot.position.y >= 0) return false;
    advance_motion(shot,cannon,context,random);
    return true;
  }};
  unsigned int constexpr initial_steps{5}, steps_between_samples{8};
  for(unsigned int step{0}; step != initial_steps; ++step) advance();
  // The first displayed sample follows the warm-up and another eight steps.
  std::array<vec3<int32_t>,5> result;
  for(unsigned int sample{0}; sample != result.size(); ++sample) {
    for(unsigned int step{0}; step != steps_between_samples; ++step) if(!advance()) break;
    result[sample] = shot.position;
  }
  return result;
}

} // namespace tfx::weapons
