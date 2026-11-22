#include "osl_resource.h"
#include <algorithm>
#include <utility>

namespace tfx::audio {

osl_resource::osl_resource(resources::bytes source) : bytes{std::move(source)} {
  resources::binary_view const data{bytes};
  data.slice(0, 0x48);
  std::array<uint8_t, 4> constexpr signature{'O', 'S', 'L', '1'};
  if(!std::ranges::equal(data.slice(0, 4), signature)) throw std::runtime_error{"music resource is not OSL1"};
  if(device() != osl_device::fm && device() != osl_device::lapc && device() != osl_device::sound_canvas) throw std::runtime_error{"unsupported OSL device"};
  data.slice(0x48, song_count() * 4u + 2u);
}
osl_device osl_resource::device() const {
  return static_cast<osl_device>(bytes[7]);
}
unsigned int osl_resource::song_count() const {
  return resources::binary_view{bytes}.le16(0x26);
}
osl_song osl_resource::song(unsigned int const index) const {
  if(index >= song_count()) throw std::out_of_range{"OSL song index"};
  auto const offset{resources::binary_view{bytes}.le32(0x48 + index * 4)};
  resources::binary_view{bytes}.slice(offset, 0x150);
  return {
    .resource{bytes},
    .offset{offset},
  };
}
unsigned int osl_resource::instrument_count() const {
  return resources::binary_view{bytes}.le16(0x48 + song_count() * 4);
}
std::span<uint8_t const> osl_resource::instrument(unsigned int const index) const {
  if(index >= instrument_count()) throw std::out_of_range{"OSL instrument index"};
  resources::binary_view const data{bytes};
  auto const start{data.le32(0x4a + song_count() * 4 + index * 4)};
  if(!start) return {};
  // Original relocation skips the stored four-byte instrument prefix. The
  // remaining common header precedes the selected device's patch payload.
  return data.slice(start + 4, bytes.size() - start - 4);
}
std::span<uint8_t const> osl_resource::instrument_parameters(unsigned int const index) const {
  std::span<uint8_t const> data;
  if(index < instrument_count()) {
    data = instrument(index);
  } else if(index <= 255) {
    // The original lookup is unchecked. An unused channel's initial index 255
    // can address a relocated pattern pointer beyond the instrument table.
    // Preserve that alias and its two-byte pattern prefix, rather than treating
    // the pattern as an ordinary instrument with a four-byte prefix.
    auto const position{0x4au + song_count() * 4u + index * 4u};
    resources::binary_view const source{bytes};
    for(unsigned int song_index{0}; song_index != song_count(); ++song_index) {
      auto const entry{song(song_index)};
      auto const table{entry.offset + 0x150};
      if(position < table || position >= table + entry.pattern_count() * 4u || (position - table) % 4) continue;
      auto const pointer{source.le32(position)};
      if(pointer) data = source.slice(pointer + 2, bytes.size() - pointer - 2);
      break;
    }
  }
  if(data.empty()) return {};
  // Native 0x16a5 skips the six-byte parameter-record header when its count
  // is nonzero. The zero-count form still returns the original record base.
  return data.subspan(resources::binary_view{data}.le16(0) ? 6u : 0u);
}
unsigned int osl_song::channels() const {
  return resources::binary_view{resource}.le16(offset + 0x12);
}
unsigned int osl_song::order_count() const {
  return resources::binary_view{resource}.le16(offset + 0x14);
}
unsigned int osl_song::loop_order() const {
  return resources::binary_view{resource}.le16(offset + 0x10);
}
unsigned int osl_song::pattern_count() const {
  return resources::binary_view{resource}.le16(offset + 0x4e);
}
uint16_t osl_song::clock_rate() const {
  return resources::binary_view{resource}.le16(offset + 0x2a);
}
uint8_t osl_song::speed() const {
  return resources::binary_view{resource}.byte(offset + 0x2c);
}
uint8_t osl_song::transpose() const {
  return resources::binary_view{resource}.byte(offset + 0x2e);
}
uint8_t osl_song::channel_volume(unsigned int const channel) const {
  if(channel >= osl_channels) throw std::out_of_range{"OSL channel index"};
  return resources::binary_view{resource}.byte(offset + 0x16 + channel);
}
uint8_t osl_song::order(unsigned int const index) const {
  if(index >= 256) throw std::out_of_range{"OSL order index"};
  return resources::binary_view{resource}.byte(offset + 0x50 + index);
}
osl_pattern osl_song::pattern(unsigned int const index) const {
  if(index >= pattern_count()) throw std::out_of_range{"OSL pattern index"};
  resources::binary_view const data{resource};
  auto const start{data.le32(offset + 0x150 + index * 4)};
  if(!start) throw std::runtime_error{"OSL pattern has no resource pointer"};
  auto const flags{data.le16(start + 2)};
  bool const packed{(flags & 0x8000) != 0};
  auto const body{start + (packed ? 6u : 4u)};
  return {
    .bytes{data.slice(body, resource.size() - body)},
    .flags{flags},
    .rows{static_cast<uint16_t>(flags & (packed ? 0x0fff : 0x7fff))},
    .stored_channels{packed ? data.le16(start + 4) : static_cast<uint16_t>(osl_channels)},
  };
}
osl_pattern_reader::osl_pattern_reader(osl_pattern const pattern) : pattern{pattern} {}
unsigned int osl_pattern_reader::offset() const {
  return position;
}
unsigned int osl_pattern_reader::rows_read() const {
  return row;
}
osl_row osl_pattern_reader::next(unsigned int const channels) {
  if(channels > osl_channels || pattern.stored_channels > osl_channels) throw std::runtime_error{"OSL pattern exceeds sixteen channels"};
  if(row == pattern.rows) throw std::out_of_range{"OSL pattern row"};
  resources::binary_view const data{pattern.bytes};
  auto const byte{[&]{return data.byte(position++);}};
  osl_row result{};
  if(!(pattern.flags & 0x8000)) {
    for(auto &cell : result) {
      for(auto &note : cell.notes) note = byte();
      cell.instrument = byte();
      cell.effect = byte();
      cell.argument = byte();
      cell.auxiliary = byte();
    }
  } else {
    unsigned int const mask_bytes{pattern.flags & 0x4000 ? std::max(1u, (pattern.stored_channels + 3u) / 4u) : 4u};
    uint32_t mask{0};
    for(unsigned int index{0}; index != mask_bytes; ++index) mask |= static_cast<uint32_t>(byte()) << (index * 8);
    // The native mask-width table reserves one byte even for an empty row.
    mask <<= (4u - mask_bytes) * 8;
    for(unsigned int channel{0}; channel != pattern.stored_channels; ++channel, mask <<= 2) {
      auto &cell{result[channel]};
      switch(mask >> 30) {
      case 0:
        break;
      case 1:
        cell.effect = byte();
        cell.argument = byte();
        break;
      case 2:
        cell.notes[0] = byte();
        cell.instrument = byte();
        break;
      case 3:
        cell.notes[0] = byte();
        if(!(pattern.flags & 0x1000)) {
          for(unsigned int note{1}; note != 4; ++note) cell.notes[note] = byte();
        }
        cell.instrument = byte();
        cell.effect = byte();
        cell.argument = byte();
        if(!(pattern.flags & 0x2000)) cell.auxiliary = byte();
        break;
      }
    }
  }
  // The native song owns only its declared channel states. Extra stored
  // columns still consume bytes, but are not exposed as additional voices.
  for(unsigned int channel{channels}; channel != osl_channels; ++channel) result[channel] = {};
  ++row;
  return result;
}

} // namespace tfx::audio
