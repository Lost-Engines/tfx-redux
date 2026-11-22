#pragma once

#include <optional>
#include "cockpit/view_control.h"

namespace tfx::render {

enum class palette_bank {
  normal,
  infrared
};
struct palette_selection_state {
  bool normal_changed{true};
  bool infrared{false};
};
struct palette_context {
  cockpit::view_mode view{cockpit::view_mode::cockpit};
  bool projectiles_present{false};
  bool infrared_available{true};
  bool night{false};
  bool night_vision{false};
};
class palette_selection {
private:
  palette_selection_state state;

public:
  explicit palette_selection(palette_selection_state initial = {});
  palette_selection_state const &value() const;
  void invalidate_normal();
  std::optional<palette_bank> advance(palette_context const &context);
};

} // namespace tfx::render
