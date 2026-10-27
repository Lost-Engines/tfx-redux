#include <iostream>
#include <stdexcept>
#include "cockpit/designator_mouse.h"

void require(bool ok,char const *message) { if(!ok) throw std::runtime_error{message}; }
int main() try {
  using tfx::cockpit::designator_mouse;
  tfx::sensors::designator base{.pitch{5000},.heading{1000}};
  auto state{base};designator_mouse input;
  input.move({100,-20});input.apply(state,false);
  require(state.heading>base.heading && state.pitch<base.pitch,"mouse right/up did not aim right/up");
  auto split{base};designator_mouse divided;
  for(int i=0;i<100;++i) { divided.move({1,-.2});divided.apply(split,false); }
  require(std::abs(int(split.heading)-int(state.heading))<=1 && std::abs(int(split.pitch)-int(state.pitch))<=1,"mouse aim depends on event/frame divisions");
  auto zoomed{base};zoomed.zoom=9;designator_mouse fine;
  fine.move({100,-20});fine.apply(zoomed,false);
  require(zoomed.heading>base.heading && zoomed.heading<state.heading,"zoom did not give finer camera aiming");
  input.reset();state=base;
  for(int i=0;i<3;++i) { input.scroll(.25);input.apply(state,false); }
  require(state.zoom==0,"fractional wheel scrolled too early");
  input.scroll(.25);input.apply(state,false);require(state.zoom==1,"fractional wheel movement was lost");
  input.scroll(10000);input.apply(state,false);require(state.zoom==10,"wheel exceeded maximum zoom");
  input.scroll(-10000);input.apply(state,false);require(state.zoom==0,"wheel wrapped below minimum zoom");
  state=base;input.move({120,-45});input.scroll(2);input.apply(state,true);
  require(state.pitch==base.pitch && state.heading==base.heading && state.zoom==2,"mouse disturbed a locked designation or tracking zoom failed");
  input.apply(state,false);require(state.pitch==base.pitch && state.heading==base.heading,"discarded tracking motion jumped after unlock");
  input.move({900,900});input.scroll(5);input.reset();auto saved{state};input.apply(state,false);
  require(state.pitch==saved.pitch && state.heading==saved.heading && state.zoom==saved.zoom,"focus/view reset retained stale camera input");
  state=base;input.move({100000,-100000});input.apply(state,false);
  require(state.pitch==80,"mouse crossed the upper ground-search limit");
  input.move({0,100000});input.apply(state,false);require(state.pitch==32767,"mouse pitch wrapped at its lower limit");
  state=base;input.move({100000,100000});input.apply(state,false);
  tfx::sensors::aim_designator(state,{10000,-2000,10000},{0,0,0},tfx::flight::player_aircraft::nighthawk,{});
  require(std::abs(int(tfx::math::signed_word(state.heading)))<=0x2000 && state.pitch<=0x2000,"mouse bypassed the F-117 gimbal limits");
  std::cout<<"Designator mouse: directions, split input, zoom sensitivity, fractional wheel, bounds, tracking and focus/view reset passed\n";
} catch(std::exception const &e) { std::cerr<<e.what()<<'\n';return 1; }
