#pragma once

#include <filesystem>
#include <string_view>
#include "flight/control_input.h"
#include "simulation/timing.h"

namespace tfx::configuration {

unsigned int constexpr configuration_buffer_size{1024};

enum class g_effects : uint8_t { none, reduced, realistic };
struct realism {
  g_effects g_force{g_effects::none};
  uint8_t precipitation{1};
  uint8_t crashing{1};
  uint8_t gear_damage{1};
  uint8_t wind{0};
  uint8_t temperature{1};
  uint8_t recovery{1};
};
struct settings {
  flight::control_source control{flight::control_source::keyboard};
  flight::control_source preferred_control{flight::control_source::keyboard}; // Startup CONTROL, restored when leaving Arcade.
  simulation::flight_model model{simulation::flight_model::arcade};
  realism effects{};
  float joystick_scale{512};
  uint16_t hud_green{125};
  int32_t irq_base{135};
  uint8_t detail{2};
  uint8_t sound_effects{0};
  uint8_t music{0};
  uint8_t stick_type{0};
  uint8_t language{0};
  uint8_t low_memory{0};
};

// Native 0xa65f0 and the relevant setting readers. Unknown names and empty
// values are ignored; keyword case and byte/word truncation match the original.
void apply(settings &state, std::string_view key, std::string_view value);
// Complete file-reader semantics, including fallback and the coupled SB mode.
settings parse(std::string_view text);
// Load saved DOS choices unchanged. A missing file starts the port with
// Simple/keyboard; empty or oversized files retain the native parser fallback.
// Hardware calibration stays external.
settings load(std::filesystem::path const &directory);

} // namespace tfx::configuration
