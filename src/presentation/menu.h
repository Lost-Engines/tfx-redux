#pragma once

#include "presentation/program.h"

namespace tfx::presentation {

struct menu_assignment {
  std::string variable;
  uint32_t value;
};
struct menu_choice {
  vec2<int16_t> minimum;
  vec2<int16_t> maximum;
  std::string label;
  std::vector<menu_assignment> assignments;
};
class menu_program {
public:
  std::string backdrop;
  std::string animation;
  uint16_t cursor_style{0};
  bool flash_box{true};
  std::vector<menu_choice> choices;
  explicit menu_program(std::string_view source);
  std::optional<unsigned int> hit_test(vec2<int16_t> const &position) const;
  void choose(unsigned int index, variables &state) const;
};

} // namespace tfx::presentation
