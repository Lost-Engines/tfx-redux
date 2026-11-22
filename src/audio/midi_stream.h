#pragma once

#include <array>
#include <vector>
#include "audio/midi_receiver.h"

namespace tfx::audio {

// Assemble the actual OSL UART byte stream. Invalid/interrupted messages are
// discarded at their interruption; complete SysEx is forwarded unchanged.
class midi_stream {
private:
  midi_receiver &receiver;
  std::vector<uint8_t> system;
  std::array<uint8_t, 2> data{};
  uint8_t status{0};
  uint8_t count{0};

public:
  explicit midi_stream(midi_receiver &receiver);
  void reset();
  void byte(uint8_t value);
};

} // namespace tfx::audio
