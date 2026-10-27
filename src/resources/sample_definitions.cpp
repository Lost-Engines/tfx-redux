#include "audio/sample_bank.h"
#include <limits>

namespace tfx::audio {

std::vector<sample_definition> load_sample_definitions(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr data_file{0xedaa4}, table{0x9860}, record_bytes{26}, maximum_slots{256};
  uint16_t constexpr centred_qsound_phase{16u << 8};
  std::vector<sample_definition> result;
  for(unsigned int index{0}; index != maximum_slots; ++index) {
    auto const at{data_file + table + index * record_bytes};
    auto const name_offset{source.le32(at)};
    if(!name_offset) return result;
    std::string name;
    for(unsigned int letter{0}; source.byte(data_file + name_offset + letter); ++letter) {
      if(letter == 8) throw std::runtime_error{"native sample name exceeds DOS filename length"};
      name += static_cast<char>(source.byte(data_file + name_offset + letter));
    }
    auto const effect{source.le32(at + 8)};
    result.push_back({
      .name{std::move(name)},
      .rate{source.le16(at + 4)},
      .retrigger_delay{source.signed_le16(at + 6)},
      .qsound_parameters{effect ? sample_pan{
        .position{source.le16(data_file + effect)},
        .increment{source.signed_le16(data_file + effect + 2)},
        .movement_enable{source.le16(data_file + effect + 4)},
      } : sample_pan{
        .position{centred_qsound_phase}, // Native null-definition fallback is the centre position.
        .increment{},
        .movement_enable{},
      }},
    });
  }
  throw std::runtime_error{"unterminated native sample definitions"};
}

} // namespace tfx::audio
