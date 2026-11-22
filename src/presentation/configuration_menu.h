#pragma once

#include <vector>
#include "configuration/settings.h"
#include "presentation/dialogue.h"

namespace tfx::presentation {

unsigned int constexpr recording_filename_limit{18};                           // Native filename widget at DATA 13768.
struct configuration_menu_assets {
  std::array<std::string, 7> main_labels;
  std::array<std::string, 6> realism_labels;
  std::array<std::string, 3> sources;
  std::array<std::string, 3> details;
  std::array<std::string, 3> g_levels;
  std::array<std::string, 4> models;
  std::array<std::string, 2> switches;
  std::array<std::string, 2> quit_lines;
  std::string title;
  std::string main_title;
  std::string realism_title;
  std::string quit;
  std::string resume;
  std::string recording;
  std::string recording_name;
  std::string no;
  std::string yes;
  std::string enrol;
  std::string missing_joystick;
  std::string missing_keyboard;
  std::string unavailable_off;
  std::string unavailable_on;
  std::string recording_marker;
  std::string recording_play;
  std::array<std::array<uint8_t, 128>, 2> recording_keys;
  explicit configuration_menu_assets(std::span<uint8_t const> executable);
};
struct configuration_audio {
  bool sound_available{false};
  bool music_available{false};
  bool sound_enabled{false};
  bool music_enabled{false};
};
enum class configuration_page {
  main,
  realism,
  confirm_quit,
  confirm_eject
};
enum class configuration_action {
  none,
  resume,
  quit,
  enrol,
  eject,
  graphics
};
enum class configuration_item {
  source,
  sensitivity,
  detail,
  model,
  sound,
  music,
  recovery,
  g_force,
  precipitation,
  crashing,
  gear_damage,
  wind,
  temperature,
  recording,
  quit,
  resume,
  change_page,
  cancel_quit,
  confirm_quit,
  enrol,
  eject,
  cancel_eject,
  confirm_eject,
  graphics
};
struct configuration_button {
  dialogue_widget drawing;
  configuration_item item;
  uint8_t value{0};
  bool enabled{true};
};

class configuration_menu {
private:
  configuration::settings &preferences;
  configuration_menu_assets const &assets;
  widget_font const &font;
  configuration_page current{configuration_page::main};
  std::vector<configuration_button> buttons;
  std::string &video_name;
  bool editing_video_name{false};
  configuration_audio *audio;
  bool graphics_enabled;
  dialogue_form form;
  void rebuild();

public:
  configuration_menu(configuration::settings &preferences, configuration_menu_assets const &assets, widget_font const &font, std::string &recording_name, configuration_audio *audio = nullptr, bool graphics_enabled = false);
  configuration_page page() const;
  void request_quit();
  std::span<configuration_button const> widgets() const;
  configuration_action select(unsigned int index);
  configuration_action click(vec2<int16_t> const &position);
  void press(uint8_t scan, bool shift = false);
  bool entering_recording_name() const;
  void draw(render::framebuffer &target) const;
};

} // namespace tfx::presentation
