#include "mouse_orbit.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace tfx::cockpit {

bool mouse_orbit::supports(view_mode const mode) {
  switch(mode) {
  case view_mode::chase: case view_mode::unbanked_chase:
  case view_mode::fly_past: case view_mode::tactical:
  case view_mode::missile: case view_mode::target:
  case view_mode::ejection: case view_mode::impact_orbit:
    return true;
  default: return false;
  }
}
bool mouse_orbit::select(view_mode const mode) {
  if(mode != selected) reset();
  selected = mode;
  return supports(mode);
}
void mouse_orbit::reset() { heading = pitch = 0; }
void mouse_orbit::move(vec2d const &delta) {
  if(!supports(selected)) return;
  double constexpr sensitivity{.004}, limit{std::numbers::pi * 85 / 180};
  heading = std::remainder(heading + delta.x * sensitivity,2 * std::numbers::pi);
  pitch = std::clamp(pitch + delta.y * sensitivity,-limit,limit);
}
render::camera mouse_orbit::apply(render::camera view, view_camera_context const &context,
                                 missile_camera_pose const &last_missile) const {
  if(!supports(selected) || (heading == 0 && pitch == 0)) return view;
  vec3<int32_t> target_q8{context.position_q8};
  if(selected == view_mode::target) {
    if(!context.observed_target_q8) return view;
    target_q8 = vec3<int32_t>{*context.observed_target_q8};
  } else if(selected == view_mode::impact_orbit) {
    if(!context.impact_target_q8) return view;
    target_q8 = vec3<int32_t>{*context.impact_target_q8};
  } else if(selected == view_mode::missile) {
    target_q8 = vec3<int32_t>{context.missile ? context.missile->position_q8 : last_missile.position_q8};
  }
  vec3d const target{vec3d{target_q8} / 256.0};
  auto const normalise{[](vec3d const &v) { return v / std::sqrt(v.dot(v)); }};
  auto const forward{normalise(view.forward)};
  auto const right{normalise(view.right - forward * view.right.dot(forward))};
  auto const down{forward.cross(right)};
  double const sh{std::sin(heading)}, ch{std::cos(heading)};
  double const sp{std::sin(pitch)}, cp{std::cos(pitch)};
  auto const rotate{[&](vec3d const &v) {
    double const x{v.dot(right)}, y{v.dot(down)}, z{v.dot(forward)};
    double const py{cp * y - sp * z}, pz{sp * y + cp * z};
    return right * (ch * x - sh * pz) + down * py + forward * (sh * x + ch * pz);
  }};
  // Rotate the eye and its complete basis about the same pivot. The subject
  // retains its screen position and distance, including during native smoothing.
  view.position = target + rotate(view.position - target);
  view.right = rotate(view.right);
  view.down = rotate(view.down);
  view.forward = rotate(view.forward);
  view.world_anchor = vec3<int32_t>{view.position};
  return view;
}

} // namespace tfx::cockpit
