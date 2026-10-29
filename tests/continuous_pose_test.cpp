#include "cockpit/view_camera.h"
#include "cockpit/attitude_projection.h"
#include "cockpit/designator.h"
#include "models/shape.h"
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
tfx::models::shape helper_shape(tfx::math::angle const heading) {
  std::vector<uint8_t> source(50);
  auto store=[&](unsigned at,uint16_t value){source.at(at)=static_cast<uint8_t>(value>>8);source.at(at+1)=static_cast<uint8_t>(value);};
  auto emit=[&](std::initializer_list<uint16_t> words){for(auto value:words){source.push_back(static_cast<uint8_t>(value>>8));source.push_back(static_cast<uint8_t>(value));}};
  store(0,32768);store(2,32767);store(34,14);store(48,65535);
  emit({98,0,0,0,0,97,1,0});
  emit({73,0,heading,0,0,0,0,0,0,0,0,4});
  emit({0});
  emit({98,1,0,0,1000,97,1,1,51,200,1,0,65535});
  return tfx::models::shape{source};
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
  // HUD positions remain continuous across the old 64-word phase boundaries,
  // including the negative-pitch wrap. Check known level and banked geometry.
  vec2d const hud_origin{1920,1080};
  double const focal{1920/std::tan(28*pi/180)};
  require((cockpit::pitch_mark_position(3,{0,0},hud_origin,focal)-hud_origin).length()<1e-10,"zero pitch mark is not on the horizon");
  auto const ten{cockpit::pitch_mark_position(4,{0,0},hud_origin,focal)-hud_origin};
  require(std::abs(ten.x)<1e-10 && ten.y>600 && ten.y<650,"ten-degree pitch mark uses the wrong projection scale");
  auto const banked{cockpit::pitch_mark_position(4,{0,pi/2},hud_origin,focal)-hud_origin};
  require(std::abs(banked.x-ten.y)<1e-10 && std::abs(banked.y)<1e-10,"banked HUD rotates in the wrong direction");
  double previous_y=0,largest_hud_step=0;
  for(int step=-128;step<=192;++step) {
    double pitch=step*2*pi/65536;
    auto point=cockpit::pitch_mark_position(3,{-pitch,0},hud_origin,focal);
    if(step>-128){require(point.y<previous_y,"HUD pitch ladder stuck on a phase boundary");largest_hud_step=std::max(largest_hud_step,previous_y-point.y);}
    require(cockpit::pitch_mark_first(pitch)+4<42,"wrapped HUD mark escaped its authored table");
    previous_y=point.y;
  }
  require(largest_hud_step<.4,"HUD still moves by whole low-resolution pixels");

  for(auto mode:{cockpit::view_mode::chase,cockpit::view_mode::unbanked_chase,cockpit::view_mode::missile}) {
    cockpit::view_control controls{{.current{mode},.distance{80}}};
    cockpit::view_camera camera;
    cockpit::view_camera_context context;
    context.position={1000,-1000,1000};context.position_q8={256000,-256000,256000};
    context.position_f32={1000,-1000,1000};
    double last_forward=0,last_x=0;
    for(unsigned step=0;step<192;++step) {
      vec3<math::angle> const angles{0,static_cast<math::angle>(step),0};
      context.basis=stored(render::continuous_rotation(angles));context.angles=context.basis.angles();
      context.missile.emplace(cockpit::missile_camera_pose{.position_q8{vec3<int32_t>{context.position_q8}},.angles{vec3<math::angle>{angles}}});
      require(camera.advance(controls,context),"external camera did not advance");
      auto view=camera.display_camera({1920,1080});
      if(step>32) {
        require(view.forward.x>last_forward && view.forward.x-last_forward<.0002,"external angle remains phase-quantised");
        require(view.position.x<last_x && last_x-view.position.x<.02,"camera setback remains phase-quantised");
      }
      last_forward=view.forward.x;last_x=view.position.x;
      require(view.forward.dot(view.forward)>.999999999,"external camera basis changes object scale");
    }
  }
  {
    // Fractional target movement must survive the fly-past camera's bearing
    // conversion even when the native integer position does not change.
    cockpit::view_control controls{{.current{cockpit::view_mode::fly_past},.fixed_position{0,-1000,0}}};
    cockpit::view_camera camera;
    cockpit::view_camera_context context;
    context.position={0,-1000,1000};context.position_q8={0,-256000,256000};context.position_f32={0,-1000,1000};
    context.speed=100000;
    double last=0;
    for(unsigned step=0;step<64;++step) {
      context.position_f32.x=static_cast<float>(step)/100;
      camera.advance(controls,context);
      auto view=camera.display_camera({1920,1080});
      if(step)require(view.forward.x>last,"fractional fly-past target movement was lost");
      last=view.forward.x;
    }
  }
  {
    cockpit::view_control controls{{.current{cockpit::view_mode::missile_infrared}}};
    cockpit::view_camera camera;
    cockpit::view_camera_context context;
    context.missile.emplace(cockpit::missile_camera_pose{.position_q8{100000,-20000,300000},.angles{13,8003,0}});
    camera.advance(controls,context);auto previous=camera.display_camera({1920,1080});
    context.missile.reset();camera.advance(controls,context);auto retained=camera.display_camera({1920,1080});
    require(previous.position==retained.position && previous.forward==retained.forward,"lost missile changed the retained display pose");
  }
  {
    sensors::designator designator;
    double last=0;
    for(unsigned step=0;step<128;++step) {
      designator.heading=static_cast<math::angle>(step);
      auto view=cockpit::designator_display_camera(designator,{1920,1080});
      if(step)require(view.forward.x>last,"designator camera retained the coarse phase table");
      last=view.forward.x;
    }
  }
  {
    models::shape_context native;
    native.camera.emplace(models::shape_camera{.basis{math::rotation_words({0,0,0})},.origin{0,0,100000}});
    double last=0;
    vec3d last_native;
    for(math::angle step=1;step<64;++step) {
      auto const shape=helper_shape(step);
      auto geometry=models::interpret(shape,{}),original=models::interpret(shape,native);
      require(geometry.warnings.empty() && geometry.primitives.size()==1 && original.warnings.empty(),"helper fixture did not produce a point");
      auto const &point=geometry.primitives[0].vertices[0].position;
      auto const &native_point=original.primitives[0].vertices[0].position;
      if(step>1) {
        require(point.x>last && point.x-last<.2,"nested model rotation lost sub-phase precision");
        require(native_point==last_native,"native helper arithmetic changed");
      }
      last=point.x;last_native=vec3d{native_point};
    }
  }
  std::cout << "4K slow turn: maximum adjacent-frame displacement " << largest_display_step << " px, native " << largest_native_step << " px; axes, wrapping, tracking roll, camera offsets, HUD ladder, designator, nested helpers and paused reads passed\n";
}
