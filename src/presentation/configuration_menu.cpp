#include "configuration_menu.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
#include "math/floating_environment.h"

namespace tfx::presentation {

configuration_menu_assets::configuration_menu_assets(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr data{0xedaa4};
  auto const string{[&](unsigned int const offset) {
    std::string value;
    for(unsigned int index{0}; index != 128; ++index) {
      auto const byte{source.byte(data + offset + index)};
      if(!byte) return value;
      value.push_back(static_cast<char>(byte));
    }
    throw std::runtime_error{"unterminated configuration menu label"};
  }};
  for(unsigned int row{0}; row != main_labels.size(); ++row) main_labels[row] = string(source.le32(data + 0x8c24 + row * 4));
  for(unsigned int row{0}; row != realism_labels.size(); ++row) realism_labels[row] = string(source.le32(data + 0x8c0c + row * 4));
  sources = {string(0x2490),string(0x2484),string(0x2468)};
  details = {string(0x261c),string(0x2620),string(0x2628)};
  g_levels = {string(0x252c),string(0x2534),string(0x253c)};
  models = {string(0x2630),string(0x2638),string(0x253c),string(0x2640)};
  switches = {string(0x2548),string(0x254c)};
  quit_lines = {string(0x24b4),string(0x24cc)};
  title = string(0x2578);
  main_title = string(0x256c);
  realism_title = string(0x258c);
  quit = string(0x2550);
  resume = string(0x255c);
  recording = string(0x2658);
  recording_name = string(0x8868);
  recording_marker = string(source.le32(data + 0x594c));
  recording_play = string(0x1344);
  for(unsigned int shifted{0}; shifted != recording_keys.size(); ++shifted)
    std::ranges::copy(source.slice(data + (shifted ? 0xad85 : 0xab50),recording_keys[shifted].size()),recording_keys[shifted].begin());
  no = string(0x24ac);
  yes = string(0x24b0);
  enrol = string(0x249c);
  missing_joystick = string(0x2470);
  missing_keyboard = string(0x247c);
  unavailable_off = string(0x2650);
  unavailable_on = string(0x2654);
}

configuration_menu::configuration_menu(configuration::settings &preferences, configuration_menu_assets const &assets, widget_font const &font, std::string &recording_name, configuration_audio *const audio) :
  preferences{preferences}, assets{assets}, font{font}, video_name{recording_name}, audio{audio} {
  rebuild();
}
configuration_page configuration_menu::page() const { return current; }
void configuration_menu::request_quit() { current = configuration_page::confirm_quit;rebuild(); }
std::span<configuration_button const> configuration_menu::widgets() const { return buttons; }
void configuration_menu::rebuild() {
  buttons.clear();
  form = {};
  auto const add{[&](std::string const &text, uint8_t const x, uint8_t const y, configuration_item const item, uint8_t const value = 0, bool const selected = true, bool const enabled = true) {
    buttons.push_back({
      .drawing{
        .position{x,y},
        .size{static_cast<uint8_t>(font.measure(text) / dialogue_cell_pixels + 1),1},
        .text{text},
        .foreground{static_cast<uint8_t>(selected ? 15 : 12)},
        .background{static_cast<uint8_t>(selected ? 11 : 7)},
        .border{selected ? dialogue_border::raised : dialogue_border::sunken},
      },
      .item{item},
      .value{value},
      .enabled{enabled},
    });
  }};
  if(current == configuration_page::confirm_eject) {
    form = {.position{11,9},.size{17,6},.border{dialogue_border::raised}};
    add(assets.yes,2,3,configuration_item::confirm_eject);
    buttons.back().drawing.size.x = 3;
    add(assets.no,9,3,configuration_item::cancel_eject);
    buttons.back().drawing.size.x = 3;
    return;
  }
  if(current == configuration_page::confirm_quit) {
    form = {
      .position{11,9},
      .size{17,8},
      .border{dialogue_border::raised},
    };
    add(assets.yes,2,2,configuration_item::confirm_quit);
    buttons.back().drawing.size.x = 3;
    add(assets.no,9,2,configuration_item::cancel_quit);
    buttons.back().drawing.size.x = 3;
    add(assets.enrol,2,5,configuration_item::enrol);
    buttons.back().drawing.size.x = 10;
    return;
  }
  if(current == configuration_page::main) {
    bool const arcade{preferences.model == simulation::flight_model::arcade};
    add(arcade ? assets.missing_joystick : assets.sources[1],10,3,configuration_item::source,1,preferences.control == flight::control_source::joystick,!arcade);
    buttons.back().drawing.size.x = static_cast<uint8_t>(font.measure(assets.sources[1]) / dialogue_cell_pixels + 1);
    add(arcade ? assets.sources[2] : assets.missing_keyboard,16,3,configuration_item::source,2,preferences.control == flight::control_source::mouse,arcade);
    buttons.back().drawing.size.x = static_cast<uint8_t>(font.measure(assets.sources[2]) / dialogue_cell_pixels + 1);
    add(arcade ? assets.missing_joystick : assets.sources[0],20,3,configuration_item::source,0,preferences.control == flight::control_source::keyboard,!arcade);
    buttons.back().drawing.size.x = static_cast<uint8_t>(font.measure(assets.sources[0]) / dialogue_cell_pixels + 1);
    add("<",10,5,configuration_item::sensitivity,0);
    std::array<char,64> scale{};
    // Native menu labels truncate, while the persisted JOYSCALE value rounds.
    float const value{preferences.joystick_scale};
    int32_t const integer{!std::isfinite(value) || value < -2147483648.f || value >= 2147483648.f ? std::numeric_limits<int32_t>::min() : static_cast<int32_t>(value)};
    std::snprintf(scale.data(),scale.size(),"%3d",integer);
    add(scale.data(),12,5,configuration_item::sensitivity,2);
    add(">",15,5,configuration_item::sensitivity,1);
    for(unsigned int index{0}; index != 3; ++index) add(assets.details[index],static_cast<uint8_t>(index == 0 ? 10 : index == 1 ? 13 : 18),7,configuration_item::detail,static_cast<uint8_t>(index),preferences.detail == index);
    std::array<simulation::flight_model,4> constexpr models{simulation::flight_model::arcade,simulation::flight_model::simple,simulation::flight_model::realistic,simulation::flight_model::military_spec};
    for(unsigned int index{0}; index != models.size(); ++index) add(assets.models[index],static_cast<uint8_t>(index == 0 ? 10 : index == 1 ? 15 : index == 2 ? 20 : 26),9,configuration_item::model,std::to_underlying(models[index]),preferences.model == models[index]);
    for(unsigned int row{0}; row != 2; ++row) {
      bool const available{audio && (row ? audio->music_available : audio->sound_available)};
      bool const enabled{audio && (row ? audio->music_enabled : audio->sound_enabled)};
      for(unsigned int value{0}; value != 2; ++value) add(available ? assets.switches[value] : value ? assets.unavailable_on : assets.unavailable_off,
        static_cast<uint8_t>(10 + value * 3),static_cast<uint8_t>(11 + row * 2),row ? configuration_item::music : configuration_item::sound,
        static_cast<uint8_t>(value),available && enabled == (value != 0),available);
    }
    for(unsigned int index{0}; index != 2; ++index) add(assets.switches[index],static_cast<uint8_t>(10 + index * 3),15,configuration_item::recovery,static_cast<uint8_t>(index),preferences.effects.recovery == index);
  } else {
    for(unsigned int index{0}; index != 3; ++index) add(assets.g_levels[index],static_cast<uint8_t>(index == 0 ? 10 : index == 1 ? 14 : 19),3,configuration_item::g_force,static_cast<uint8_t>(index),std::to_underlying(preferences.effects.g_force) == index);
    std::array<uint8_t,5> const values{preferences.effects.precipitation,preferences.effects.crashing,preferences.effects.gear_damage,preferences.effects.wind,preferences.effects.temperature};
    std::array<configuration_item,5> constexpr items{configuration_item::precipitation,configuration_item::crashing,configuration_item::gear_damage,configuration_item::wind,configuration_item::temperature};
    for(unsigned int row{0}; row != items.size(); ++row) for(unsigned int index{0}; index != 2; ++index) add(assets.switches[index],static_cast<uint8_t>(10 + index * 3),static_cast<uint8_t>(5 + row * 2),items[row],static_cast<uint8_t>(index),values[row] == index);
  }
  add("EJECT",32,17,configuration_item::eject);
  add(assets.quit,5,17,configuration_item::quit);
  add(assets.resume,13,17,configuration_item::resume);
  add(current == configuration_page::main ? assets.realism_title : assets.main_title,23,17,configuration_item::change_page);
}
configuration_action configuration_menu::select(unsigned int const index) {
  if(index >= buttons.size()) return configuration_action::none;
  auto const item{buttons[index].item};
  uint8_t const value{buttons[index].value};
  if(!buttons[index].enabled) return configuration_action::none;
  switch(item) {
    case configuration_item::source: preferences.control = static_cast<flight::control_source>(value);break;
    case configuration_item::sensitivity:
      if(value != 2) {
        math::floating_environment const environment;
        preferences.joystick_scale = static_cast<float>(static_cast<long double>(preferences.joystick_scale) + (value ? 10. : -10.));
        // Native comparison ordering sends unordered input to the upper limit.
        if(!(preferences.joystick_scale <= 999)) preferences.joystick_scale = 999;
        if(preferences.joystick_scale < 0) preferences.joystick_scale = 0;
      }
      break;
    case configuration_item::detail: preferences.detail = value;break;
    case configuration_item::model: {
      auto const next{static_cast<simulation::flight_model>(value)};
      if(next != preferences.model) {
        if(next == simulation::flight_model::arcade) preferences.control = flight::control_source::mouse;
        else if(preferences.model == simulation::flight_model::arcade) preferences.control = preferences.preferred_control;
        preferences.model = next;
      }
      break;
    }
    case configuration_item::sound: if(audio) audio->sound_enabled = value != 0;break;
    case configuration_item::music: if(audio) audio->music_enabled = value != 0;break;
    case configuration_item::recovery: preferences.effects.recovery = value;break;
    case configuration_item::g_force: preferences.effects.g_force = static_cast<configuration::g_effects>(value);break;
    case configuration_item::precipitation: preferences.effects.precipitation = value;break;
    case configuration_item::crashing: preferences.effects.crashing = value;break;
    case configuration_item::gear_damage: preferences.effects.gear_damage = value;break;
    case configuration_item::wind: preferences.effects.wind = value;break;
    case configuration_item::temperature: preferences.effects.temperature = value;break;
    case configuration_item::change_page: current = current == configuration_page::main ? configuration_page::realism : configuration_page::main;break;
    case configuration_item::quit: current = configuration_page::confirm_quit;break;
    case configuration_item::cancel_quit: current = configuration_page::main;break;
    case configuration_item::confirm_quit: return configuration_action::quit;
    case configuration_item::enrol: return configuration_action::enrol;
    case configuration_item::resume: return configuration_action::resume;
    case configuration_item::eject: current = configuration_page::confirm_eject;break;
    case configuration_item::cancel_eject: current = configuration_page::main;break;
    case configuration_item::confirm_eject: return configuration_action::eject;
    default: break;
  }
  rebuild();
  return configuration_action::none;
}
configuration_action configuration_menu::click(vec2<int16_t> const &position) {
  dialogue_widget const recording_field{
    .position{18,13},
    .size{10,1},
  };
  editing_video_name = current == configuration_page::main && dialogue_contains(form,recording_field,position);
  if(editing_video_name) return configuration_action::none;
  for(unsigned int index{0}; index != buttons.size(); ++index) if(dialogue_contains(form,buttons[index].drawing,position)) return select(index);
  return configuration_action::none;
}
bool configuration_menu::entering_recording_name() const { return editing_video_name; }
void configuration_menu::press(uint8_t const scan, bool const shift) {
  if(!editing_video_name) return;
  if(scan == 0x1c) { editing_video_name = false;return; }
  if(scan == 0x0e) { if(!video_name.empty()) video_name.pop_back();return; }
  if(scan >= assets.recording_keys[0].size() || video_name.size() >= recording_filename_limit) return;
  uint8_t character{assets.recording_keys[shift][scan]};
  // D09AF bypasses the lowercase call used by the unshifted branch.
  // Host function/navigation keys do not insert NULs into a C++ path string.
  if(character < ' ' || character == 127) return;
  if(!shift && character >= 'A' && character <= 'Z') character = static_cast<uint8_t>(character + ('a' - 'A'));
  video_name.push_back(static_cast<char>(character));
}
void configuration_menu::draw(render::framebuffer &target) const {
  draw_dialogue_form(target,form);
  for(auto const &button : buttons) draw_dialogue_widget(target,font,form,button.drawing);
  if(current == configuration_page::confirm_eject) {
    int const centre{(form.position.x + 1) * dialogue_cell_pixels + form.size.x * 4};
    font.draw(target,{centre - font.measure("EJECT?") / 2,82},"EJECT?",dialogue_palette_base + 15);
    font.draw(target,{centre - font.measure("LEAVE AIRCRAFT") / 2,94},"LEAVE AIRCRAFT",dialogue_palette_base + 15);
    return;
  }
  if(current == configuration_page::confirm_quit) {
    auto const centre{(form.position.x + 1) * dialogue_cell_pixels + form.size.x * 4};
    for(unsigned int row{0}; row != assets.quit_lines.size(); ++row) font.draw(target,{centre - font.measure(assets.quit_lines[row]) / 2,(form.position.y + 1) * dialogue_cell_pixels + (row ? 9 : 2)},assets.quit_lines[row],dialogue_palette_base + 15);
    return;
  }
  draw_dialogue_widget(target,font,form,{
    .position{16,static_cast<uint8_t>(current == configuration_page::main ? 11 : 5)},
    .size{17,static_cast<uint8_t>(current == configuration_page::main ? 5 : 9)},
    .background{8},
    .border{dialogue_border::outline},
  });
  if(current == configuration_page::main) {
    auto const text{video_name + (editing_video_name ? "_" : "")};
    std::string_view shown{text};
    while(!shown.empty() && font.measure(shown) > 10 * dialogue_cell_pixels) shown.remove_prefix(1);
    draw_dialogue_widget(target,font,form,{
      .position{18,13},
      .size{10,1},
      .text{shown},
      .background{8},
      .border{dialogue_border::sunken},
      .left_aligned{true},
    });
    font.draw(target,{158,125},assets.recording,dialogue_palette_base + 15);
  }
  font.draw(target,{161 - font.measure(assets.title) / 2,27},assets.title,dialogue_palette_base);
  font.draw(target,{160 - font.measure(assets.title) / 2,26},assets.title,dialogue_palette_base + 15);
  auto const &subtitle{current == configuration_page::main ? assets.main_title : assets.realism_title};
  font.draw(target,{161 - font.measure(subtitle) / 2,34},subtitle,dialogue_palette_base);
  font.draw(target,{160 - font.measure(subtitle) / 2,33},subtitle,dialogue_palette_base + 15);
  auto const labels{[&](auto const &lines) {
    for(unsigned int row{0}; row != lines.size(); ++row) font.draw(target,{78 - font.measure(lines[row]),58 + static_cast<int>(row) * 16},lines[row],dialogue_palette_base + 15);
  }};
  if(current == configuration_page::main) labels(assets.main_labels);
  else labels(assets.realism_labels);
}

} // namespace tfx::presentation
