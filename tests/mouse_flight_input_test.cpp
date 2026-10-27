#include "flight/mouse_input.h"
#include "flight/aerodynamic_controls.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, char const *message) { if(!ok) throw std::runtime_error{message}; }
double roll_demand(vec2<int16_t> const &mouse) {
  tfx::flight::aerodynamic_control_state controls{
    .minimum_elevator{-1},.maximum_elevator{1},.aileron_limit{1},
  };
  tfx::flight::aerodynamic_powerplant powerplant;
  tfx::flight::update_aerodynamic_controls(controls,powerplant,{
    .source{tfx::flight::control_source::mouse},.mouse{vec2<int16_t>{mouse}},
  });
  return controls.aileron;
}
}
int main() try {
  using tfx::flight::mouse_flight_input;
  // Reproduce the original frame-size amplification through the real cubic
  // aerodynamic control law, then check the same input with Redux saturation.
  for(int speed : {600,1200,4000}) for(int fps : {60,30,15,10}) for(int sign : {-1,1}) {
    mouse_flight_input original, corrected;
    int previous{0};double peak_original{0}, peak_corrected{0};
    for(int frame{1};frame<=fps;++frame) {
      int const position{speed*frame/fps};
      auto const delta{static_cast<int16_t>(sign*(position-previous))};previous=position;
      auto const native{original.advance({delta,delta})};
      auto const bounded{corrected.advance_clamped({delta,delta})};
      require(bounded.x>=-512 && bounded.x<=511 && bounded.y==bounded.x,"diagonal mouse movement escaped the stick range");
      peak_original=std::max(peak_original,std::abs(roll_demand(native)));
      peak_corrected=std::max(peak_corrected,std::abs(roll_demand(bounded)));
    }
    require(peak_corrected<=1,"low frame rate overdrives full aileron deflection");
    if(speed==4000 && fps<=15) require(peak_original>4,"regression does not reproduce the original low-frame-rate overdrive");
    require(corrected.position.x==(sign<0 ? -512 : 511),"maximum stick travel depends on frame rate");
    auto const before{corrected.position};
    corrected.advance_clamped({static_cast<int16_t>(-sign),static_cast<int16_t>(-sign)});
    require(corrected.position.x==before.x-sign && corrected.position.y==before.y-sign,"overshoot swallowed reverse movement");
    std::cout<<speed<<" pixels/s, "<<fps<<" fps, direction "<<sign<<": native peak "<<peak_original<<", Redux "<<peak_corrected<<'\n';
  }
  mouse_flight_input large;
  large.advance_clamped({32767,-32768});
  require(large.position.x==511 && large.position.y==-512,"large diagonal input wrapped");
  large.advance_clamped({32767,-32768});
  require(large.position.x==511 && large.position.y==-512,"repeated large input flipped steering");
  large.advance_clamped({-32768,32767});
  require(large.position.x==-512 && large.position.y==511,"large reversal did not saturate both axes");
  mouse_flight_input split, whole;
  for(int n=0;n<20;++n) split.advance_clamped({4,-7});
  whole.advance_clamped({80,-140});
  require(split.position.x==whole.position.x && split.position.y==whole.position.y,"ordinary mouse sensitivity depends on event batching");
  mouse_flight_input native, bounded;
  for(auto const &delta : {vec2<int16_t>{20,-40},vec2<int16_t>{-12,8},vec2<int16_t>{40,60}}) {
    auto const a{native.advance(delta)},b{bounded.advance_clamped(delta)};
    require(a.x==b.x && a.y==b.y,"ordinary in-range mouse steering changed");
  }
  std::cout<<"Mouse stick saturation: frame-rate sweeps, real aerodynamic demand, diagonal limits, overflow, reversal and ordinary sensitivity passed\n";
} catch(std::exception const &e) { std::cerr<<e.what()<<'\n';return 1; }
