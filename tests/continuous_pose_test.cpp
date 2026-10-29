#include "cockpit/view_camera.h"
#include "render/continuous_pose.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, char const *message) {
  if(!value) throw std::runtime_error{message};
}
tfx::math::attitude stored(mat3d const &matrix) {
  auto const column{[&](vec3d const &unit) {
    auto const value{matrix*unit*32767};
    return vec3<int32_t>{static_cast<int32_t>(std::lround(value.x)),static_cast<int32_t>(std::lround(value.y)),static_cast<int32_t>(std::lround(value.z))};
  }};
  return {.right{column({1,0,0})},.down{column({0,1,0})},.forward{column({0,0,1})}};
}
}
int main() {
  using namespace tfx;
  for(int p: {-8192,0,4096,16384}) for(int h:{0,8192,32000,49152}) for(int r:{0,4096,16384,49152}) {
    vec3<math::angle> const angles{static_cast<math::angle>(p),static_cast<math::angle>(h),static_cast<math::angle>(r)};
    auto const precise{render::continuous_rotation(angles)}, native{math::rotation(angles)};
    for(unsigned i=0;i<9;++i) require(std::abs(precise.data[i]-native.data[i])<.0002,"continuous matrix differs in axis convention");
    auto const decoded{render::continuous_basis(stored(precise))};
    auto const right{decoded*vec3d{1,0,0}},down{decoded*vec3d{0,1,0}},forward{decoded*vec3d{0,0,1}};
    require(std::abs(right.dot(down))<1e-12 && std::abs(forward.dot(forward)-1)<1e-12,"stored basis lost orthonormality");
    auto const rebuilt{render::continuous_rotation(render::continuous_angles(decoded))};
    for(unsigned i=0;i<9;++i) require(std::abs(rebuilt.data[i]-decoded.data[i])<.0002,"Euler recovery flipped axes or handedness");
  }
  double const pi{std::numbers::pi};
  auto const seam{render::follow_angles({0,pi-.01,0},{0,-pi+.01,0},.25,false)};
  require(std::abs(seam.y-(pi-.005))<1e-12,"follow filter went the long way around wraparound");
  auto const rear{render::follow_angles({0,0,0},{0,pi,0},.25,false)};
  require(std::abs(rear.y+pi/4)<1e-12,"rearward transition stalled or reversed the native half-turn direction");
  auto const tracked{render::follow_angles({0,0,0},{.1,.2,.3},.25,true)};
  require(std::abs(tracked.z-.3)<1e-12,"tracking roll acquired an extra lag");

  double largest_native_step=0,largest_display_step=0;
  for(auto mode:{cockpit::view_mode::cockpit,cockpit::view_mode::full_forward,cockpit::view_mode::virtual_cockpit}) {
    cockpit::view_control controls{{.current{mode}}};
    cockpit::view_camera camera;
    cockpit::view_camera_context context;
    context.position={1000,-1000,1000};context.position_q8={256000,-256000,256000};
    vec3d const landmark{1000,-1000,11000};
    double last_display=0,last_native=0;
    for(unsigned step=0;step<192;++step) {
      // Aircraft basis moves through three native sine-table bins in one-word
      // increments. Converting it back to native angles used to lose these.
      context.basis=stored(render::continuous_rotation(vec3<math::angle>{0,static_cast<math::angle>(step),0}));
      context.angles=context.basis.angles();
      require(camera.advance(controls,context),"camera did not advance");
      auto display{camera.display_camera({1920,1080})}, native{camera.last_pose().camera({1920,1080})};
      for(auto *view:{&display,&native}) { view->projection=render::projection_mode::continuous;view->focal_distance=1920/std::tan(28*pi/180); }
      auto const x{display.project(display.transform(landmark)).x};
      auto const old_x{native.project(native.transform(landmark)).x};
      if(step>20) {
        require(x<last_display,"slow camera turn stuck on an angle-table step");
        largest_display_step=std::max(largest_display_step,std::abs(x-last_display));
        largest_native_step=std::max(largest_native_step,std::abs(old_x-last_native));
      }
      last_display=x;last_native=old_x;
      auto const again{camera.display_camera({1920,1080})};
      require(again.forward==display.forward,"reading a paused display advanced the follow filter");
      require(camera.last_pose().position_q16==native.projector().position_q16,"display altered native camera translation");
    }
  }
  require(largest_display_step<.6 && largest_native_step>10,"4K slow-turn regression did not distinguish sub-phase movement");
  std::cout << "4K slow turn: maximum adjacent-frame displacement " << largest_display_step << " px, native " << largest_native_step << " px; axes, wrapping, tracking roll and paused reads passed\n";
}
