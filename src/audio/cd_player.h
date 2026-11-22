#pragma once

#include <filesystem>
#include <memory>
#include <span>

namespace tfx::audio {

// Streams a CUE/BIN image or numbered local WAV/FLAC/Vorbis/MP3 tracks.
// Track numbers are physical CD numbers: track 1 is normally game data.
class cd_player {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  cd_player(std::filesystem::path const &source, unsigned int output_rate);
  ~cd_player();
  cd_player(cd_player const &) = delete;
  cd_player &operator=(cd_player const &) = delete;
  bool contains(unsigned int track) const;
  bool play(unsigned int track);
  void stop();
  bool playing() const;
  void render(std::span<float> stereo);
};

} // namespace tfx::audio
