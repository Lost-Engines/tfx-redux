#include <iostream>
#include <stdexcept>
#include "platform/pause_input.h"

void require(bool ok,char const *message){if(!ok)throw std::runtime_error{message};}
int main() try {
  using tfx::platform::pause_input;
  pause_input input;pause_input::snapshot keys{};input.synchronise(keys,true);
  for(int key : {GLFW_KEY_ENTER,GLFW_KEY_KP_ENTER,GLFW_KEY_SPACE,GLFW_KEY_A,GLFW_KEY_UP,GLFW_KEY_P,GLFW_KEY_PAUSE}) {
    keys.fill(false);input.update(keys,false,true);keys[static_cast<size_t>(key)]=true;
    require(input.update(keys,true,true),"ordinary key did not resume");
    require(input.consumed(key),"resume press leaked into flight controls");
    require(!input.update(keys,false,true) && input.consumed(key),"held resume key was not suppressed");
    keys.fill(false);input.update(keys,false,true);keys[static_cast<size_t>(key)]=true;
    require(!input.update(keys,false,true) && !input.consumed(key),"fresh gameplay press stayed suppressed");
  }
  for(int key : {GLFW_KEY_LEFT_ALT,GLFW_KEY_LEFT_SHIFT,GLFW_KEY_LEFT_CONTROL,GLFW_KEY_LEFT_SUPER,GLFW_KEY_ESCAPE,GLFW_KEY_GRAVE_ACCENT,GLFW_KEY_M,GLFW_KEY_4,GLFW_KEY_CAPS_LOCK,GLFW_KEY_F1,GLFW_KEY_F5,GLFW_KEY_F10,GLFW_KEY_F25}) {
    keys.fill(false);input.update(keys,false,true);keys[static_cast<size_t>(key)]=true;
    require(!input.update(keys,true,true) && !input.consumed(key),"modifier or modal shortcut resumed flight");
  }
  keys.fill(false);input.update(keys,false,true);keys[GLFW_KEY_SPACE]=true;input.update(keys,false,true);
  require(!input.update(keys,true,true),"key held before pause resumed flight");
  keys.fill(false);input.update(keys,true,false);keys[GLFW_KEY_ENTER]=true;
  require(!input.update(keys,true,false) && !input.update(keys,true,true),"unfocused or refocus key resumed flight");
  keys.fill(false);input.update(keys,true,true);keys[GLFW_KEY_ENTER]=true;keys[GLFW_KEY_SPACE]=true;
  require(input.update(keys,true,true) && input.consumed(GLFW_KEY_ENTER) && input.consumed(GLFW_KEY_SPACE),"simultaneous resume keys were not consumed");
  std::cout<<"Pause resume: ordinary keys, held fire suppression, release/repress, focus, camera and modal exceptions passed\n";
} catch(std::exception const &e){std::cerr<<e.what()<<'\n';return 1;}
