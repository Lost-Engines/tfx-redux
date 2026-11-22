#pragma once

#include <fstream>
#include <map>
#include "recording/file.h"

namespace tfx::recording {

// Stream frames rather than retaining a whole flight in memory. Only the
// current scene and the original bounded name dictionary remain resident.
class recorder {
private:
  std::ofstream output;
  file dictionary;
  std::map<std::string, uint16_t> handles;
  uint16_t frames{0};

public:
  recorder();
  ~recorder();
  recorder(recorder const &) = delete;
  auto operator=(recorder const &)->recorder & = delete;
  void begin(std::filesystem::path const &path);
  bool active() const;
  uint16_t frame_count() const;
  void append(scene_frame &source);
  void finish();
};

} // namespace tfx::recording
