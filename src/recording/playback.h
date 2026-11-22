#pragma once

#include <fstream>
#include <map>
#include <optional>
#include "recording/file.h"

namespace tfx::recording {

class playback {
private:
  std::ifstream input;
  file_header header;
  uint16_t remaining;
  uint32_t position{header_bytes};
  std::map<uint16_t, std::string> names;
  std::array<uint8_t, maximum_frame_bytes> buffer{};

public:
  explicit playback(std::filesystem::path const &path);
  uint16_t frame_count() const;
  std::optional<scene_frame> next();
};

} // namespace tfx::recording
