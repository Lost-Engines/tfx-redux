#include "capture_scene.h"
#ifndef NDEBUG
#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace tfx::platform {
namespace {
double scalar(std::istringstream &line, double low=-1000000, double high=1000000) {
  double value;
  if(!(line >> value) || !std::isfinite(value) || value<low || value>high)
    throw std::runtime_error{"invalid or out-of-range number"};
  return value;
}
unsigned int integer(std::istringstream &line, unsigned int high) {
  double const n{scalar(line,0,high)};
  if(n!=std::floor(n)) throw std::runtime_error{"expected an unsigned integer"};
  return static_cast<unsigned int>(n);
}
vec3d point(std::istringstream &line) { return {scalar(line),scalar(line),scalar(line)}; }
vec3<math::angle> angles(std::istringstream &line) {
  auto const angle{[&] { return static_cast<math::angle>(static_cast<int32_t>(std::lround(scalar(line,-360,360)*65536/360))); }};
  return {angle(),angle(),angle()};
}
std::string word(std::istringstream &line) {
  std::string value;
  if(!(line >> value)) throw std::runtime_error{"missing value"};
  return value;
}
} // namespace
capture_scene::capture_scene(std::filesystem::path const &path) {
  if(std::filesystem::file_size(path)>65536) throw std::runtime_error{"capture recipe exceeds 64 KiB"};
  std::ifstream input{path};
  if(!input) throw std::runtime_error{"cannot open capture recipe"};
  unsigned int row{0};
  for(std::string text;std::getline(input,text);) {
    ++row;
    text.resize(text.find('#')==std::string::npos ? text.size() : text.find('#'));
    std::istringstream line{text};
    std::string key;
    if(!(line>>key)) continue;
    try {
      if(key=="position") {
        auto const p{point(line)};
        if(p.x<0 || p.x>=409600 || p.z<0 || p.z>=409600 || p.y>0 || p.y<-8192)
          throw std::runtime_error{"player must lie inside the map, Y between -8192 and 0"};
        position=vec3<int32_t>{p};
      } else if(key=="angles") this->angles=platform::angles(line);
      else if(key=="speed") speed=static_cast<float>(scalar(line,.05,2));
      else if(key=="aircraft") {
        auto const name{word(line)};
        if(name=="efa") aircraft=flight::player_aircraft::eurofighter;
        else if(name=="f117") aircraft=flight::player_aircraft::nighthawk;
        else if(name=="f22") aircraft=flight::player_aircraft::raptor;
        else throw std::runtime_error{"aircraft expects efa, f117 or f22"};
      } else if(key=="view") {
        auto const name{word(line)};
        if(name=="cockpit") view=cockpit::view_mode::cockpit;
        else if(name=="virtual") view=cockpit::view_mode::virtual_cockpit;
        else if(name=="external") view=cockpit::view_mode::chase;
        else if(name=="left") view=cockpit::view_mode::look_left;
        else if(name=="right") view=cockpit::view_mode::look_right;
        else if(name=="rear") view=cockpit::view_mode::rear;
        else throw std::runtime_error{"view expects cockpit, virtual, external, left, right or rear"};
      } else if(key=="look") look={scalar(line,-180,180),scalar(line,-80,80)};
      else if(key=="camera") {
        auto const eye{point(line)}, target{point(line)};
        double const roll{scalar(line,-180,180)*std::numbers::pi/180};
        auto forward{target-eye};
        if(forward.length()<.01) throw std::runtime_error{"camera eye and target coincide"};
        forward/=forward.length();
        auto right{vec3d{0,1,0}.cross(forward)};
        if(right.length()<.001) throw std::runtime_error{"camera cannot look exactly vertically"};
        right/=right.length();
        auto const down{forward.cross(right)};
        camera=render::camera{
          .position{vec3d{eye}},.right{right*std::cos(roll)+down*std::sin(roll)},
          .down{down*std::cos(roll)-right*std::sin(roll)},.forward{vec3d{forward}},
          .near_distance{1},.world_anchor{vec3<int32_t>{eye}},
        };
      } else if(key=="hud") {
        hud=word(line);
        if(hud!="default" && hud!="nav" && hud!="guns" && hud!="air" && hud!="ground")
          throw std::runtime_error{"hud expects default, nav, guns, air or ground"};
      } else if(key=="displays") displays={integer(line,8),integer(line,2),integer(line,8)};
      else if(key=="warmup") warmup_frames=integer(line,500);
      else if(key=="mission-actors") mission_actors=integer(line,1)!=0;
      else if(key=="hide-player") hide_player=integer(line,1)!=0;
      else if(key=="player-event") player_events.push_back(integer(line,255));
      else if(key=="at") {
        capture_event event{};
        event.frame=integer(line,500);
        auto const target{word(line)};
        if(target=="player-event") event.target=capture_event_target::player;
        else if(target=="actor-event") {
          event.target=capture_event_target::actor;event.actor=integer(line,63);
        } else if(target=="terrain-event") {
          event.target=capture_event_target::terrain;
          event.x=integer(line,199);event.z=integer(line,199);
        } else throw std::runtime_error{"at expects player-event, actor-event or terrain-event"};
        event.entry=integer(line,255);
        events.push_back(event);
      }
      else if(key=="actor") {
        if(actors.size()==64) throw std::runtime_error{"at most 64 staged actors"};
        auto name{word(line)};
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
          throw std::runtime_error{"invalid SSD definition name"};
        auto const p{point(line)};
        auto const a{platform::angles(line)};
        actors.push_back({name,vec3d{p},vec3<math::angle>{a},integer(line,255)});
      } else throw std::runtime_error{"unknown key: "+key};
      std::string extra;
      if(line>>extra) throw std::runtime_error{"unexpected trailing value"};
    } catch(std::exception const &error) {
      throw std::runtime_error{path.string()+":"+std::to_string(row)+": "+error.what()};
    }
  }
  for(auto const &event : events) {
    if(event.frame>warmup_frames) throw std::runtime_error{"capture event occurs after final frame"};
    if(event.target==capture_event_target::actor && event.actor>=actors.size()) throw std::runtime_error{"capture event refers to missing actor"};
  }
  if(camera && view!=cockpit::view_mode::chase) throw std::runtime_error{"capture camera requires view external"};
}
void capture_scene::initialise(missions::setup &setup) const {
  if(position) { setup.start.position=*position;setup.start.landed=false; }
  if(angles) setup.angles=*angles;
  if(aircraft) setup.aircraft=*aircraft;
  if(speed) setup.speed_factor=*speed;
}
} // namespace tfx::platform
#endif
