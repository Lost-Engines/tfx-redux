#include <iostream>
#ifndef NDEBUG
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include "platform/capture_scene.h"

namespace {
void require(bool value,char const *message) { if(!value) throw std::runtime_error{message}; }
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("tfx-capture-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() { std::filesystem::create_directory(path); }
  ~temporary_directory() { std::filesystem::remove_all(path); }
};
}
int main() try {
  temporary_directory temporary;
  auto const path{temporary.path/"pose.scene"};
  auto const load{[&](std::string const &text) {
    { std::ofstream file{path};file<<text; }
    return tfx::platform::capture_scene{path};
  }};
  auto const capture{load("# A banked external camera\nposition 10000 -500 20000\nangles 0 90 -30\naircraft f22\nview external\ncamera 9990 -510 20020 10000 -500 20000 23\nactor mig_29 10000 -500 19970 0 90 -30 0\nmission-actors 0\ndisplays 8 2 8\n")};
  tfx::missions::setup setup{};setup.start.landed=true;setup.weapons=7;
  capture.initialise(setup);
  require(setup.start.position==vec3<int32_t>{10000,-500,20000} && !setup.start.landed,"sub-tile airborne pose was not retained");
  require(setup.aircraft==tfx::flight::player_aircraft::raptor && setup.angles.y==16384 && setup.weapons==7,"staging changed mission stores or lost pose");
  require(!capture.mission_actors && capture.actors.size()==1,"actor staging was lost");
  auto const timed{load("warmup 40\nat 0 terrain-event 137 113 61\nat 25 actor-event 0 5\nat 40 player-event 3\nactor f_22 10000 -500 20000 0 0 0 0\n")};
  require(timed.events.size()==3 && timed.events[0].x==137 && timed.events[0].z==113 && timed.events[0].entry==61,"terrain timeline coordinates were lost");
  require(timed.events[1].frame==25 && timed.events[1].actor==0 && timed.events[2].frame==40,"capture event timing was lost");
  require(load("view left").view==tfx::cockpit::view_mode::look_left && load("view right").view==tfx::cockpit::view_mode::look_right && load("view rear").view==tfx::cockpit::view_mode::rear,"native side views were not selected");
  auto const &c{*capture.camera};
  require(std::abs(c.forward.length()-1)<1e-12 && std::abs(c.down.length()-1)<1e-12 && std::abs(c.right.length()-1)<1e-12,"camera basis is not normalised");
  require(std::abs(c.forward.dot(c.down))<1e-12 && std::abs(c.forward.dot(c.right))<1e-12 && std::abs(c.down.dot(c.right))<1e-12,"rolled camera axes are not perpendicular");
  auto const target{c.transform({10000,-500,20000})};
  require(std::abs(target.x)<1e-10 && std::abs(target.y)<1e-10 && target.z>0,"look-at camera missed its subject");
  for(auto const *text : {
    "at 25 player-event 3", "at 0 actor-event 0 3", "at 0 terrain-event 200 1 3", "at 0 nonsense 3",
    "positon 1 2 3", "position nan 0 0", "position 0 -9000 0", "position 409600 0 0",
    "view nonsense", "warmup 2.5", "warmup 501", "hud bogus", "displays 0 3 0",
    "view external\ncamera 0 0 0 0 0 0 0", "view external\ncamera 0 0 0 0 1 0 0",
    "camera 0 0 0 1 0 0 0", "actor ../test 0 0 0 0 0 0 0", "hide-player 0 junk",
  }) {
    bool rejected=false;
    try { load(text); } catch(std::runtime_error const&) { rejected=true; }
    require(rejected,"invalid capture recipe was accepted");
  }
  std::cout<<"Capture recipes, bounds, and rolled look-at camera passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr<<error.what()<<'\n';return 1; }
#else
int main() { std::cout<<"Capture staging is absent from Release builds\n"; }
#endif
