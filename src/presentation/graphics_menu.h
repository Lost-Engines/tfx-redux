#pragma once

#include "configuration/graphics.h"
#include "presentation/dialogue.h"

namespace tfx::presentation {

enum class graphics_action { none, cancel, apply };
enum class graphics_key { enter, escape, backspace, tab, left, right, up, down, erase };
class graphics_menu {
  widget_font const &font;
  configuration::graphics_settings pending;
  dialogue_form form{};
  unsigned int selected{0};
  bool editing{false}, replace_text{false}, field_changed{false}, resize_window{false};
  std::string text, error;
  graphics_action action{graphics_action::none};
  bool accept_field();
  void adjust(int direction);
  void edit(unsigned int row);
public:
  graphics_menu(widget_font const &font, configuration::graphics_settings const &settings);
  void click(vec2<int16_t> const &point);
  void character(unsigned int codepoint);
  void press(graphics_key key, bool shift=false);
  void draw(render::framebuffer &image) const;
  graphics_action result() const;
  configuration::graphics_settings const &value() const;
  bool window_resize_requested() const;
  void failed(std::string message);
};
} // namespace tfx::presentation
