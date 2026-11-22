#include "sample_bank.h"

namespace tfx::audio {

sample_bank::sample_bank(std::span<sample_definition const> const source, resources::archive const &game) : definitions{source.begin(),source.end()} {
  descriptors.reserve(definitions.size());
  buffers.reserve(definitions.size());
  for(auto const &definition : definitions) {
    auto const name{"samples/" + definition.name + ".sam"};
    bool const available{!definition.name.empty() && game.contains(name)};
    auto pcm{available ? game.read(name) : resources::bytes{}};
    if(pcm.size() > 65535) throw std::runtime_error{"native Sound Blaster sample exceeds its DMA buffer: " + name};
    descriptors.push_back({
      .retrigger_delay{definition.retrigger_delay},
      .samples{static_cast<uint32_t>(pcm.size())},
      .available{available},
    });
    buffers.push_back(std::move(pcm));
  }
}
std::span<sample_descriptor const> sample_bank::scheduling() const {
  return descriptors;
}
std::span<sample_definition const> sample_bank::entries() const {
  return definitions;
}
std::span<uint8_t const> sample_bank::pcm(unsigned int const index) const {
  return buffers.at(index);
}
uint16_t sound_blaster_period(uint16_t const rate) {
  if(!rate) throw std::invalid_argument{"Sound Blaster rate must not be zero"};
  uint8_t const time_constant{static_cast<uint8_t>(256 - 1'000'000 / rate)};
  return static_cast<uint16_t>(256 - time_constant);
}

} // namespace tfx::audio
