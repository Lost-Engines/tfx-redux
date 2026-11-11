#include "tactical_map.h"
#include <algorithm>
#include <array>
#include <limits>

namespace tfx::platform {

auto show_tactical_map(window &window, cockpit::tactical_map &map,
    cockpit::tactical_map_snapshot const &state, render::palette const &colours)->void {
  auto *const host{window.get()};
  std::array<bool,GLFW_KEY_LAST + 1> held{};
  for(int key{GLFW_KEY_SPACE}; key <= GLFW_KEY_LAST; ++key) held[static_cast<unsigned int>(key)] = window.key_down(key);
  map.open(state);
  window.title("TFX Redux — Tactical map");
  double previous{glfwGetTime()}, accumulated{0};
  while(window.poll()) {
    double const now{glfwGetTime()};
    double const seconds{std::max(0.0,now - previous)};
    previous = now;
    if(!window.focused()) { glfwWaitEventsTimeout(.05);continue; }
    glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_HIDDEN);
    bool dismiss{false};
    for(int key{GLFW_KEY_SPACE}; key <= GLFW_KEY_LAST; ++key) {
      // Alt is reserved for host shortcuts, including Alt+Enter.
      if(key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) continue;
      bool const down{window.key_down(key)};
      dismiss |= down && !held[static_cast<unsigned int>(key)];
      held[static_cast<unsigned int>(key)] = down;
    }
    if(dismiss) break;
    accumulated += seconds * simulation::timer_hz;
    auto const ticks{static_cast<simulation::timer_ticks>(std::min(accumulated,static_cast<double>(std::numeric_limits<simulation::timer_ticks>::max())))};
    accumulated -= ticks;
    map.advance(ticks);
    window.present(map.image().colourise(colours));
    glfwWaitEventsTimeout(.01);
  }
  glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
}

} // namespace tfx::platform
