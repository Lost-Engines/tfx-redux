#include "configuration.h"
#include <iostream>
#include "configuration/save.h"
#include "platform/keyboard.h"
#include "presentation/graphics_menu.h"

namespace tfx::platform {
namespace {
bool configure_graphics(window &window, resources::executable_tables const &tables,
    configuration::graphics_settings &settings, std::filesystem::path const &directory,
    render::framebuffer const &background, render::palette const &palette) {
  presentation::graphics_menu menu{tables.interface_font(),settings};
  auto *const host{window.get()};auto *const previous_input{glfwGetWindowUserPointer(host)};
  glfwSetWindowUserPointer(host,&menu);
  auto const previous_key{glfwSetKeyCallback(host,[](GLFWwindow *host,int key,int,int action,int mods) {
    if(action!=GLFW_PRESS && action!=GLFW_REPEAT) return;
    using presentation::graphics_key;
    std::optional<graphics_key> selected;
    switch(key) {
      case GLFW_KEY_ENTER:case GLFW_KEY_KP_ENTER:selected=graphics_key::enter;break;
      case GLFW_KEY_ESCAPE:selected=graphics_key::escape;break;
      case GLFW_KEY_BACKSPACE:selected=graphics_key::backspace;break;
      case GLFW_KEY_DELETE:selected=graphics_key::erase;break;
      case GLFW_KEY_TAB:selected=graphics_key::tab;break;
      case GLFW_KEY_LEFT:selected=graphics_key::left;break;
      case GLFW_KEY_RIGHT:selected=graphics_key::right;break;
      case GLFW_KEY_UP:selected=graphics_key::up;break;
      case GLFW_KEY_DOWN:selected=graphics_key::down;break;
    }
    if(selected) static_cast<presentation::graphics_menu*>(glfwGetWindowUserPointer(host))->press(*selected,(mods&GLFW_MOD_SHIFT)!=0);
  })};
  auto const previous_character{glfwSetCharCallback(host,[](GLFWwindow *host,unsigned int c) {
    static_cast<presentation::graphics_menu*>(glfwGetWindowUserPointer(host))->character(c);
  })};
  bool held{glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS},applied{false};
  while(window.poll()) {
    if(!window.focused()) {glfwWaitEventsTimeout(.05);continue;}
    bool const mouse{glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS};
    if(mouse && !held) if(auto const pointer{window.cursor_position()}) menu.click(*pointer);
    held=mouse;
    if(menu.result()==presentation::graphics_action::cancel) break;
    if(menu.result()==presentation::graphics_action::apply) {
      try {
        auto const next{menu.value()};
        window.configure(next.video.resolution,next.video.automatic_resolution,menu.window_resize_requested() ? std::optional{next.window_scale} : std::nullopt);
        settings=next;applied=true;
        try {configuration::save_graphics(directory,settings);}
        catch(std::exception const &e) {std::cerr<<"Graphics applied for this session, but could not save "<<configuration::graphics_filename<<": "<<e.what()<<'\n';}
        break;
      } catch(std::exception const &e) {std::cerr<<e.what()<<'\n';menu.failed("COULD NOT APPLY DISPLAY SETTINGS");}
    }
    auto image{background};menu.draw(image);
    window.title("TFX Redux — Graphics");window.present(image.colourise(palette));glfwWaitEventsTimeout(.01);
  }
  glfwSetCharCallback(host,previous_character);glfwSetKeyCallback(host,previous_key);glfwSetWindowUserPointer(host,previous_input);
  return applied;
}
} // namespace

presentation::configuration_action configure_flight(window &window, resources::executable_tables const &tables,
    configuration::settings &preferences, std::string &recording_name, std::filesystem::path const &directory,
    render::framebuffer const &background, render::palette const &colours, audio::runtime *const sound, bool const quit_prompt, configuration::graphics_settings *const graphics) {
  presentation::configuration_audio audio{
    .sound_available{sound && sound->sound_available()},
    .music_available{sound && sound->music_available()},
    .sound_enabled{sound && sound->sound_enabled()},
    .music_enabled{sound && sound->music_enabled()},
  };
  presentation::configuration_menu menu{preferences,tables.configuration_widgets(),tables.interface_font(),recording_name,&audio,graphics && !quit_prompt};
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
      if(action == presentation::configuration_action::graphics && graphics) {
        if(configure_graphics(window,tables,*graphics,directory,background,palette)) {result=presentation::configuration_action::resume;break;}
        mouse_held=glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        escape_held=glfwGetKey(host,GLFW_KEY_ESCAPE)==GLFW_PRESS;
        continue;
      }
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
