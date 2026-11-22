#pragma once

#include <filesystem>
#include <memory>
#include "audio/midi_receiver.h"

namespace tfx::audio {

class la_synth final : public midi_receiver {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  la_synth(std::filesystem::path const &rom_directory, unsigned int sample_rate);
  ~la_synth() override;
  void short_message(uint32_t packed) override;
  void sysex(std::span<uint8_t const> message) override;
  void render(std::span<float> stereo) override;
  void silence();                                                              // Preserve the game's custom patches and system setup.
};

} // namespace tfx::audio
