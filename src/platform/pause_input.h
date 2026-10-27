#pragma once

#include <array>
#include <cstddef>
#include <GLFW/glfw3.h>

namespace tfx::platform {

// Resume presses are swallowed until release, including level-triggered fire
// and steering keys. Modifiers and modal/screenshot shortcuts do not resume.
class pause_input {
public:
  using snapshot = std::array<bool,GLFW_KEY_LAST+1>;
private:
  snapshot previous{}, blocked{};
  bool had_focus{false};
  static bool resumes(int key, snapshot const &keys) {
    if(key>=GLFW_KEY_LEFT_SHIFT || key==GLFW_KEY_ESCAPE || key==GLFW_KEY_GRAVE_ACCENT || key==GLFW_KEY_M || key==GLFW_KEY_4
       || key==GLFW_KEY_CAPS_LOCK || key==GLFW_KEY_NUM_LOCK || key==GLFW_KEY_SCROLL_LOCK || key==GLFW_KEY_PRINT_SCREEN) return false;
    if(key==GLFW_KEY_Q && (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT])) return false;
    return true;
  }
public:
  void synchronise(snapshot const &keys, bool focused) {
    previous=keys;had_focus=focused;
    for(size_t i=0;i<blocked.size();++i) blocked[i]=blocked[i]&&keys[i];
  }
  bool update(snapshot const &keys, bool paused, bool focused) {
    bool resume{false};
    for(int key=GLFW_KEY_SPACE;key<=GLFW_KEY_LAST;++key) {
      auto const index{static_cast<size_t>(key)};
      if(!keys[index]) blocked[index]=false;
      if(paused && focused && had_focus && keys[index] && !previous[index] && resumes(key,keys)) {
        blocked[index]=true;resume=true;
      }
    }
    previous=keys;had_focus=focused;return resume;
  }
  bool consumed(int key) const { return blocked[static_cast<size_t>(key)]; }
};

} // namespace tfx::platform
