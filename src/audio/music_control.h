#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace tfx::audio {

using lapc_order_profiles = std::array<std::array<uint8_t, 12>, 6>;

struct flight_audio_state {
  float throttle{0};
  bool engines_running{false};
  bool inhibited{false};
  bool collision{false};
  bool aircraft_disabled{false};
};

enum class music_mode : uint8_t {
  none,
  soundblaster_cd,
  lapc,
  fm,
  sound_canvas
};
struct music_control_state {
  uint8_t last_cue{0};
  int16_t resume_cue{0};
  bool engine_playing{false};
  float previous_throttle{-1};
};
struct music_control_context {
  music_mode mode{music_mode::none};
  uint8_t sound_device{0};
  bool enabled{true};
  bool music_playing{true};
  bool cd_playing{false};
  bool preselected{false};
  bool inhibited{false};
  bool collision{false};
  bool aircraft_disabled{false};
  bool engines_running{false};
  float throttle{0};
  std::array<uint8_t, 12> lapc_orders{};
};
struct music_selection {
  bool replace_resource{false};
  uint8_t resource_index{0};
  uint8_t order{0};
  bool stop_previous{false};
};

struct music_control_update {
  bool stop_and_release{false};
  std::optional<music_selection> selection{};
};

// Native 0xa5a9c and both device-family selectors. Resource replacement and OSL
// delivery are explicit caller operations; this controller owns cue progression.
std::optional<music_selection> select_flight_music(music_control_state &state,
  music_control_context const &context, uint8_t cue);

// Native 0xa5210/0xa5374 without the separately deferred CD branch. Playback
// position is supplied by the live sequencer, rather than inferred from time.
music_control_update update_flight_music(music_control_state &state,
  music_control_context const &context, bool playing);

} // namespace tfx::audio
