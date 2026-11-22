#pragma once
#include <filesystem>
#include <memory>
#include "audio/midi_receiver.h"

namespace tfx::audio {

enum class sound_canvas_model {
  sc55,
  scc1a
};

class sc55_synth final : public midi_receiver {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  sc55_synth(std::filesystem::path const &rom_directory, unsigned int sample_rate, sound_canvas_model model = sound_canvas_model::sc55);
  ~sc55_synth() override;
  void short_message(uint32_t packed) override;
  void sysex(std::span<uint8_t const> message) override;
  void render(std::span<float> stereo) override;
};

} // namespace tfx::audio
