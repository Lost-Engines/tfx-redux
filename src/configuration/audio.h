#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace tfx::configuration {

enum class music_engine : uint8_t {
  none,
  fm,
  lapc,
  scc1a,
  sc55
};
struct audio_preferences {
  // Unset entries inherit the original installation's MUSIC choice.
  std::optional<music_engine> engine{};
  std::optional<bool> cd_music{};
};
struct audio_settings {
  music_engine engine{music_engine::none};
  bool cd_music{false};
};
music_engine parse_music_engine(std::string_view name);
std::string_view music_engine_name(music_engine engine);
audio_settings resolve_audio(uint8_t native_music, audio_preferences const &preferences);

} // namespace tfx::configuration
