#include "arming.h"
#include <algorithm>
#include "presentation/arming_screen.h"

namespace tfx::platform {
namespace {

class arming_keys {
private:
  GLFWwindow *host;
  presentation::arming_screen &screen;

public:
  arming_keys(GLFWwindow *const host, presentation::arming_screen &screen) : host{host}, screen{screen} {
    glfwSetWindowUserPointer(host,this);
    glfwSetKeyCallback(host,[](GLFWwindow *const host, int const key, int const, int const action, int const) {
      if(action != GLFW_PRESS) return;
      auto &controls{*static_cast<arming_keys*>(glfwGetWindowUserPointer(host))};
      controls.screen.press(key == GLFW_KEY_ESCAPE ? 1 : key == GLFW_KEY_Y ? 0x15 : key == GLFW_KEY_N ? 0x31 : key == GLFW_KEY_ENTER ? 0x1c : 0);
    });
  }
  ~arming_keys() {
    glfwSetKeyCallback(host,nullptr);
    glfwSetWindowUserPointer(host,nullptr);
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(host,GLFW_RAW_MOUSE_MOTION,GLFW_FALSE);
    glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
  }
  arming_keys(arming_keys const&) = delete;
  arming_keys &operator=(arming_keys const&) = delete;
};

} // anonymous namespace

arming_result arm_mission(window &window, resources::archive const &game, resources::executable_tables const &tables,
  weapons::armament &inventory, flight::player_aircraft const aircraft, unsigned int const default_fit, presentation::arming_entry const entry) {
  struct restore_inventory {
    weapons::armament &inventory;
    weapons::pylon_inventory original;
    bool restore{true};
    ~restore_inventory() { if(restore) inventory.pylons = original; }
  } previous{
    .inventory{inventory},
    .original{inventory.pylons},
  };
  presentation::arming_screen screen{game,tables,inventory,aircraft,default_fit,entry,window.resolution()};
  auto *const host{window.get()};
  arming_keys keys{host,screen};
  window.title("TFX Redux — Arming");
  window.present(screen.image().colourise(screen.colours()),screen.image().size);
  double next_frame{glfwGetTime()};
  std::optional<vec2<int16_t>> previous_pointer;
  std::optional<vec2<int32_t>> previous_coordinates;
  while(window.poll()) {
    double const now{glfwGetTime()};
    if(!window.focused()) {
      next_frame = now;
      previous_pointer.reset();
      previous_coordinates.reset();
      if(glfwRawMouseMotionSupported()) glfwSetInputMode(host,GLFW_RAW_MOUSE_MOTION,GLFW_FALSE);
      glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
      glfwWaitEventsTimeout(.05);
      continue;
    }
    if(now < next_frame) { glfwWaitEventsTimeout(std::min(.01,next_frame - now));continue; }
    next_frame = now + 1.0 / 70;
    bool const rotating{screen.rotating()};
    auto const coordinates{window.cursor_coordinates()};
    bool const inside{coordinates && coordinates->x >= 0 && coordinates->x < render::screen_width && coordinates->y >= 0 && coordinates->y < render::screen_height};
    auto const pointer{inside ? std::optional{vec2<int16_t>{*coordinates}} : std::nullopt};
    auto const position{pointer ? vec2<int16_t>{*pointer} : previous_pointer ? vec2<int16_t>{*previous_pointer} : vec2<int16_t>{100,75}};
    // Quantise positions rather than individual deltas so slow movement still
    // accumulates a full game pixel at high window scales. Disabled cursor
    // coordinates remain unbounded, allowing repeated full rotations.
    auto const logical{coordinates ? std::optional{vec2<int32_t>{*coordinates}} : std::nullopt};
    auto const movement{logical && previous_coordinates ? vec2<int16_t>{*logical - *previous_coordinates} : vec2<int16_t>{0,0}};
    previous_coordinates = logical;
    previous_pointer = pointer;
    uint8_t const buttons{static_cast<uint8_t>(coordinates && (inside || rotating) ? (glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0) | (glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ? 2 : 0) : 0)};
    bool const finished{screen.advance(position,movement,buttons)};
    int const cursor_mode{screen.rotating() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_HIDDEN};
    if(glfwGetInputMode(host,GLFW_CURSOR) != cursor_mode) {
      glfwSetInputMode(host,GLFW_CURSOR,cursor_mode);
      if(glfwRawMouseMotionSupported()) glfwSetInputMode(host,GLFW_RAW_MOUSE_MOTION,screen.rotating() ? GLFW_TRUE : GLFW_FALSE);
      previous_coordinates.reset();
    }
    window.present(screen.image().colourise(screen.colours()),screen.image().size);
    if(finished) {
      if(screen.cancelled()) return arming_result::cancelled;
      previous.restore = false;
      return arming_result::accepted;
    }
  }
  return arming_result::closed;
}

} // namespace tfx::platform
