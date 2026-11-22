#pragma once

#include <string>
#include "presentation/widget_font.h"

namespace tfx::presentation {

int constexpr dialogue_cell_pixels{8};
uint8_t constexpr dialogue_palette_base{240};
enum class dialogue_border : uint8_t { double_bevel, raised, sunken = 3, outline = 110 };
struct dialogue_form {
  vec2<uint8_t> position{0,2};
  vec2<uint8_t> size{38,20};
  uint8_t background{9};
  dialogue_border border{dialogue_border::double_bevel};
};
struct dialogue_widget {
  vec2<uint8_t> position{0,0};
  vec2<uint8_t> size{1,1};
  std::string text{};
  uint8_t foreground{15};
  uint8_t background{11};
  dialogue_border border{dialogue_border::raised};
  bool left_aligned{false};
};

// Native 0xcf87c window body and the 0xcfe4c text-button/outline paths.
void draw_dialogue_form(render::framebuffer &target, dialogue_form const &form);
void draw_dialogue_widget(render::framebuffer &target, widget_font const &font, dialogue_form const &form, dialogue_widget const &widget);
bool dialogue_contains(dialogue_form const &form, dialogue_widget const &widget, vec2<int16_t> const &point);

} // namespace tfx::presentation
