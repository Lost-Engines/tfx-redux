#pragma once

#include <cstdint>

namespace tfx::flight {

enum class control_source : uint8_t {
  keyboard,
  joystick,
  mouse
};
struct direction_keys {
  bool up{false};
  bool left{false};
  bool down{false};
  bool right{false};
};

} // namespace tfx::flight
