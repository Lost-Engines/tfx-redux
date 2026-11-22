#include "midi_stream.h"
#include <stdexcept>

namespace tfx::audio {

midi_stream::midi_stream(midi_receiver &receiver) : receiver{receiver} {
  system.reserve(256);
}
void midi_stream::reset() {
  system.clear();
  status = count = 0;
}
void midi_stream::byte(uint8_t const value) {
  // MIDI real-time bytes can interrupt either kind of message without losing
  // its running status or partially collected payload.
  if(value >= 0xf8) { receiver.short_message(value);return; }
  if(!system.empty()) {
    if(value == 0xf7) { system.push_back(value);receiver.sysex(system);reset();return; }
    if(!(value & 128)) {
      unsigned int constexpr maximum_sysex_bytes{65536};
      if(system.size() == maximum_sysex_bytes) { reset();throw std::runtime_error{"MIDI SysEx exceeds the stream limit"}; }
      system.push_back(value);
      return;
    }
    reset();
  }
  if(value & 128) {
    count = 0;
    status = value;
    if(value == 0xf0) { system.push_back(value);status = 0; } else if(value == 0xf6 || value == 0xf7) { receiver.short_message(value);status = 0; } else if(value == 0xf4 || value == 0xf5) status = 0;
    return;
  }
  if(!status) return;
  data[count++] = value;
  unsigned int const length{(status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 || status == 0xf1 || status == 0xf3 ? 1u : 2u};
  if(count != length) return;
  uint32_t const message{status | (static_cast<uint32_t>(data[0]) << 8) | (length == 2 ? static_cast<uint32_t>(data[1]) << 16 : 0)};
  receiver.short_message(message);
  count = 0;
  if(status >= 0xf0) status = 0;
}

} // namespace tfx::audio
