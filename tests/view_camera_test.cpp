#include <iostream>
#include <stdexcept>
#include <utility>
#include "cockpit/view_camera.h"

int main() {
  tfx::cockpit::view_control_state initial;
  initial.current = tfx::cockpit::view_mode::missile_infrared;
  tfx::cockpit::view_control views{std::move(initial)};
  tfx::cockpit::view_camera camera;
  tfx::cockpit::view_camera_context context;
  context.missile.emplace(tfx::cockpit::missile_camera_pose{
    .position_q8{100000, -20000, 300000},
    .angles{0, 8000, 0},
  });
  if(!camera.advance(views, context)) throw std::runtime_error{"infrared camera was not handled"};
  auto const previous{camera.last_pose()};
  context.missile.reset();
  if(!camera.advance(views, context) || !views.value().ready || views.value().written != 1 || views.value().queue[0] != tfx::cockpit::view_mode::missile) throw std::runtime_error{"missing missile must hand off to the follow camera"};
  if(camera.last_pose().position_q16 != previous.position_q16 || camera.last_pose().matrix != previous.matrix) throw std::runtime_error{"missing missile must retain the last valid camera pose"};
  views.consume();
  if(!camera.advance(views, context)) throw std::runtime_error{"follow camera did not accept the lost missile"};
  std::cout << "Missing missile safely retains the last camera pose and returns to follow view\n";
}
