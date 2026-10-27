#include "cockpit/mouse_orbit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, char const *message) {
  if(!value) throw std::runtime_error{message};
}
void close(vec3d const &a, vec3d const &b, char const *message) {
  require((a-b).dot(a-b) < 1e-14,message);
}
}
int main() {
  using namespace tfx::cockpit;
  view_camera_context context;
  context.position_q8 = {25600,-128000,51200};
  context.observed_target_q8.emplace(51200,-25600,102400);
  context.impact_target_q8.emplace(-25600,-51200,76800);
  missile_camera_pose missile{.position_q8{76800,-76800,128000}};
  context.missile = missile;
  for(auto mode : {view_mode::chase,view_mode::unbanked_chase,view_mode::tactical,
                   view_mode::fly_past,view_mode::missile,view_mode::target,
                   view_mode::impact_orbit,view_mode::ejection}) {
    auto const pivot{mode == view_mode::target ? *context.observed_target_q8 :
      mode == view_mode::impact_orbit ? *context.impact_target_q8 :
      mode == view_mode::missile ? missile.position_q8 : context.position_q8};
    vec3d const target{vec3d{pivot}/256.0};
    for(double bank : {0.,.7,2.8}) {
      auto original{tfx::render::inspection_camera(target-vec3d{0,0,40},0,0)};
      original.right = {std::cos(bank),std::sin(bank),0};
      original.down = {-std::sin(bank),std::cos(bank),0};
      mouse_orbit orbit;
      require(orbit.select(mode),"external camera must accept mouse orbit");
      close(orbit.apply(original,context,missile).position,original.position,"idle camera changed");
      orbit.move({80,-45});
      auto const changed{orbit.apply(original,context,missile)};
      auto const movement{changed.position-original.position};
      require(movement.dot(original.right)>0 && movement.dot(original.down)<0,"mouse direction does not follow screen axes");
      close(changed.transform(target),original.transform(target),"orbit moved the subject on screen");
      require(std::abs((changed.position-target).dot(changed.position-target)-1600)<1e-8,"orbit changed viewing distance");
      close(changed.forward.cross(changed.right),changed.down,"camera basis lost orthogonality");
      orbit.move({-80,45});
      close(orbit.apply(original,context,missile).position,original.position,"opposite motion did not restore view");
      orbit.move({30,50});orbit.reset();
      close(orbit.apply(original,context,missile).position,original.position,"reset did not restore native view");
      orbit.move({30,50});
      require(!orbit.select(view_mode::cockpit),"cockpit must retain flight steering");
      close(orbit.apply(original,context,missile).position,original.position,"orbit leaked into cockpit");
      orbit.select(mode);
      close(orbit.apply(original,context,missile).position,original.position,"orbit leaked between views");
    }
  }
  mouse_orbit orbit;
  for(auto mode : {view_mode::full_forward,view_mode::virtual_cockpit,view_mode::target_tracking,
                   view_mode::designator,view_mode::missile_infrared,view_mode::rear})
    require(!orbit.select(mode),"internal camera must not orbit");
  orbit.select(view_mode::missile);orbit.move({100,20});
  auto const original{tfx::render::inspection_camera(vec3d{missile.position_q8}/256.0-vec3d{0,0,40},0,0)};
  auto const active{orbit.apply(original,context,missile)};
  context.missile.reset();
  close(orbit.apply(original,context,missile).position,active.position,"spent missile lost its orbit pivot");
  // A moving subject and its native follow camera translate together.
  context.missile = missile;
  context.missile->position_q8 += vec3<int32_t>{25600,0,-51200};
  auto translated{original};translated.position += vec3d{100,0,-200};
  close(orbit.apply(translated,context,missile).position,active.position+vec3d{100,0,-200},"orbit stopped following its subject");
  std::cout << "External mouse orbit: screen axes, fixed pivot/distance, tracking, reset and internal-view exclusion passed\n";
}
