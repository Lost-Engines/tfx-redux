#pragma once

#include <chrono>
#include <fstream>
#include <optional>
#include "render/framebuffer.h"

namespace tfx::recording {

struct video_frame {
  render::framebuffer image;
  render::dac_palette palette;
  std::chrono::microseconds duration;
};

// Redux captures palette indices, preserving the actual displayed view without
// the DOS scene queue's coordinate, resource, model-count or frame-count limits.
class video_recorder {
private:
  std::ofstream output;
  render::pixel_buffer previous;
  vec2<int> previous_size{0,0};
  uint64_t frames{0};

public:
  video_recorder();
  ~video_recorder();
  video_recorder(video_recorder const&) = delete;
  auto operator=(video_recorder const&)->video_recorder& = delete;
  void begin(std::filesystem::path const &path);
  bool active() const;
  uint64_t frame_count() const;
  void append(render::framebuffer const &image, render::dac_palette const &palette, std::chrono::microseconds duration);
  void finish();
};

class video_playback {
private:
  std::ifstream input;
  uint64_t frames{0}, remaining{0}, bytes_remaining{0};
  render::framebuffer previous;

public:
  explicit video_playback(std::filesystem::path const &path);
  static bool recognises(std::filesystem::path const &path);
  uint64_t frame_count() const;
  std::optional<video_frame> next();
};

} // namespace tfx::recording
