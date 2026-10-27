#pragma once

#include <array>
#include <string>
#include "audio/sample_scheduler.h"
#include "resources/archive.h"

namespace tfx::audio {

struct sample_pan {
  uint16_t position{};
  int16_t increment{};
  uint16_t movement_enable{}; // Preserve the original word; any nonzero value enables movement.
};

struct sample_definition {
  std::string name;
  uint16_t rate;
  int16_t retrigger_delay;
  sample_pan qsound_parameters;
};
std::vector<sample_definition> load_sample_definitions(std::span<uint8_t const> executable);

// Definitions retain their native slot numbers, including the empty slots.
// PCM is unsigned eight-bit mono. QSound parameters belong to the optional
// resident Q2 processor; the ordinary Sound Blaster DMA path leaves PCM intact.
class sample_bank {
private:
  std::vector<sample_definition> definitions;
  std::vector<sample_descriptor> descriptors;
  std::vector<resources::bytes> buffers;

public:
  sample_bank(std::span<sample_definition const> definitions, resources::archive const &game);
  std::span<sample_descriptor const> scheduling() const;
  std::span<sample_definition const> entries() const;
  std::span<uint8_t const> pcm(unsigned int index) const;
};

// Native DSP command 0x40 truncates the reciprocal, then keeps its low byte.
// The resulting DAC interval is an integer number of microseconds.
uint16_t sound_blaster_period(uint16_t requested_rate);

} // namespace tfx::audio
