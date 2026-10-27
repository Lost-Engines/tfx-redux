#include "configuration.h"
#include <iostream>
#include "configuration/save.h"
#include "platform/keyboard.h"

namespace tfx::platform {

presentation::configuration_action configure_flight(window &window, resources::executable_tables const &tables,
    configuration::settings &preferences, std::string &recording_name, std::filesystem::path const &directory,
    render::framebuffer const &background, render::palette const &colours, audio::runtime *const sound, bool const quit_prompt) {
  presentation::configuration_audio audio{
    .sound_available{sound && sound->sound_available()},
    .music_available{sound && sound->music_available()},
    .sound_enabled{sound && sound->sound_enabled()},
    .music_enabled{sound && sound->music_enabled()},
  };
  presentation::configuration_menu menu{preferences,tables.configuration_widgets(),tables.interface_font(),recording_name,&audio};
  if(quit_prompt) menu.request_quit();
  auto palette{colours};
  for(unsigned int shade{0}; shade != 16; ++shade) {
    auto const value{static_cast<uint8_t>((shade * 4u << 2) | (shade * 4u >> 4))};
    palette[presentation::dialogue_palette_base + shade] = {value,value,value};
  }
  auto *const host{window.get()};
  auto *const previous_input{glfwGetWindowUserPointer(host)};
  glfwSetWindowUserPointer(host,&menu);
  auto const previous_key{glfwSetKeyCallback(host,[](GLFWwindow *const window, int const key, int const, int const action, int const modifiers) {
    if(action == GLFW_PRESS || action == GLFW_REPEAT)
      static_cast<presentation::configuration_menu*>(glfwGetWindowUserPointer(window))->press(make_code(key),(modifiers & GLFW_MOD_SHIFT) != 0);
  })};
  glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
  auto result{presentation::configuration_action::quit};
  bool mouse_held{glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS};
  bool escape_held{glfwGetKey(host,GLFW_KEY_ESCAPE) == GLFW_PRESS};
  while(window.poll()) {
    if(!window.focused()) { glfwWaitEventsTimeout(.05);continue; }
    bool const escape{glfwGetKey(host,GLFW_KEY_ESCAPE) == GLFW_PRESS};
    if(escape && !escape_held) { result = presentation::configuration_action::resume;break; }
    escape_held = escape;
    if(!quit_prompt && !menu.entering_recording_name() && (glfwGetKey(host,GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(host,GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) && glfwGetKey(host,GLFW_KEY_Q) == GLFW_PRESS) break;
    bool const mouse{glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS};
    if(mouse && !mouse_held) if(auto const pointer{window.cursor_position()}) {
      auto const action{menu.click(*pointer)};
      if(quit_prompt && menu.page() != presentation::configuration_page::confirm_quit) { result = presentation::configuration_action::resume;break; }
      if(action != presentation::configuration_action::none) { result = action;break; }
    }
    mouse_held = mouse;
    if(sound) {
      // Native music-on restarts CD playback. Apply a changed switch once,
      // rather than replaying that command on every open-menu redraw.
      if(audio.sound_enabled != sound->sound_enabled()) sound->enable_sound(audio.sound_enabled);
      if(audio.music_enabled != sound->music_enabled()) sound->enable_music(audio.music_enabled);
    }
    auto image{background};
    menu.draw(image);
    window.title(menu.page() == presentation::configuration_page::confirm_eject ? "TFX Redux — Eject" : menu.page() == presentation::configuration_page::confirm_quit ? "TFX Redux — Quit" : menu.page() == presentation::configuration_page::realism ? "TFX Redux — Realism" : "TFX Redux — Configuration");
    window.present(image.colourise(palette));
    glfwWaitEventsTimeout(.01);
  }
  glfwSetKeyCallback(host,previous_key);
  glfwSetWindowUserPointer(host,previous_input);
  if(!quit_prompt && !configuration::save_preferences(directory,preferences)) std::cerr << "Flight preferences could not be saved: TFX.CFG is missing, empty or too large.\n";
  return result;
}

} // namespace tfx::platform
