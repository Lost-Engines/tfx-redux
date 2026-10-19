#include "compiled_overlay.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace tfx::resources {

compiled_overlay::compiled_overlay(std::span<uint8_t const> const bytes) {
  binary_view const source{bytes};
  movable = source.byte(0) == 0xe8;
  unsigned int constexpr plane_size{render::screen_width * render::screen_height / 4};
  unsigned int cursor{movable ? 0x57u : 0x1fu};
  std::array<uint8_t,8> registers{};
  unsigned int table{0};
  if(movable) {
    if(source.le32(0) != 0xe8 || source.byte(4) || source.le16(0x36) != 0xc381 || source.le16(0x4b) != 0xbb66 || source.le16(0x4f) != 0xb966 || source.byte(0x53) != 0xb4) throw std::runtime_error{"unknown compiled canopy header"};
    table = source.le32(0x38) + 5; // Position-independent CALL/POP supplies the five-byte origin.
    auto const bx{source.le16(0x4d)}, cx{source.le16(0x51)};
    registers[3] = static_cast<uint8_t>(bx);registers[7] = static_cast<uint8_t>(bx >> 8);
    registers[1] = static_cast<uint8_t>(cx);registers[5] = static_cast<uint8_t>(cx >> 8);
    registers[4] = source.byte(0x54);
  } else {
    if(source.le32(0) != 0x8b565755 || source.byte(4) != 0xec || source.byte(5) != 0xba || source.le32(6) != 0x3c4 || source.le16(0x15) != 0xbb66 || source.le16(0x19) != 0xb966 || source.byte(0x1d) != 0xb4) throw std::runtime_error{"unknown compiled overlay header"};
    auto const bx{source.le16(0x17)}, cx{source.le16(0x1b)};
    registers[3] = static_cast<uint8_t>(bx);registers[7] = static_cast<uint8_t>(bx >> 8);
    registers[1] = static_cast<uint8_t>(cx);registers[5] = static_cast<uint8_t>(cx >> 8);
    registers[4] = source.byte(0x1e);
  }
  std::map<unsigned int,column_entry> boundaries;
  unsigned int column{0};
  uint8_t planes{0};
  bool ended{false};
  std::vector<int64_t> column_ends;
  unsigned int constexpr artwork_budget{100000};
  for(unsigned int step{0}; step != artwork_budget; ++step) {
    boundaries.emplace(cursor,column_entry{
      .first{static_cast<unsigned int>(pixels.size())},
      .column{column},
    });
    switch(source.byte(cursor++)) {
    case 0xb0: registers[0] = source.byte(cursor++);break;
    case 0xee:
      planes = registers[0];
      if(planes > 15) throw std::runtime_error{"invalid compiled overlay plane mask"};
      break;
    case 0xc6: case 0x88: {
      auto const operation{source.byte(cursor - 1)}, operand{source.byte(cursor++)};
      if((operand & 7) != 7 || (operand >> 6) < 1 || (operand >> 6) > 2) throw std::runtime_error{"unknown compiled overlay pixel address"};
      unsigned int address{0};
      if((operand >> 6) == 1) address = static_cast<unsigned int>(static_cast<int>(std::bit_cast<int8_t>(source.byte(cursor++))));
      else { address = source.le32(cursor);cursor += 4; }
      if(address >= plane_size) throw std::runtime_error{"compiled overlay pixel exceeds its screen"};
      uint8_t colour{0};
      if(operation == 0xc6) {
        if((operand & 0x38) != 0) throw std::runtime_error{"unknown compiled overlay literal"};
        colour = source.byte(cursor++);
      } else colour = registers[(operand >> 3) & 7];
      pixels.push_back({
        .address{static_cast<uint16_t>(address)},
        .colour{colour},
        .planes{planes},
        .column{column},
      });
      break;
    }
    case 0x66:
      if(!movable || source.byte(cursor++) != 0x4d || ++column > 80) throw std::runtime_error{"unknown compiled overlay column separator"};
      break;
    case 0x0f:
      if(!movable || source.byte(cursor++) != 0x84) throw std::runtime_error{"unknown compiled overlay column end"};
      column_ends.push_back(static_cast<int64_t>(cursor) + 4 + std::bit_cast<int32_t>(source.le32(cursor)));cursor += 4;
      break;
    case 0x74:
      if(!movable) throw std::runtime_error{"unknown compiled overlay short end"};
      column_ends.push_back(static_cast<int64_t>(cursor) + 1 + std::bit_cast<int8_t>(source.byte(cursor)));++cursor;
      break;
    case 0x5e:
      if(source.byte(cursor++) != 0x5f || source.byte(cursor++) != 0x5d || source.byte(cursor++) != 0xc3) throw std::runtime_error{"unknown compiled overlay footer"};
      ended = true;
      break;
    default: throw std::runtime_error{"unknown compiled artwork token at " + std::to_string(cursor - 1)};
    }
    if(ended) break;
  }
  if(!ended) throw std::runtime_error{"compiled overlay has no bounded end"};
  if(movable) {
    for(auto const destination : column_ends) if(destination != cursor - 4) throw std::runtime_error{"compiled canopy column has an unknown end"};
    if(table < 158 || table > bytes.size() || bytes.size() - table != 160) throw std::runtime_error{"compiled canopy column table exceeds its data"};
    for(int pan{-79}; pan != 80; ++pan) {
      auto const destination{source.le16(static_cast<unsigned int>(static_cast<int>(table) + pan * 2)) + 5u};
      auto const entry{boundaries.find(destination)};
      if(entry == boundaries.end()) throw std::runtime_error{"compiled canopy entry is not an artwork boundary"};
      entries[static_cast<unsigned int>(pan + 79)] = entry->second;
    }
  } else if(cursor != bytes.size()) throw std::runtime_error{"unused compiled overlay bytes"};
}

screen_overlay compiled_overlay::image(int const horizontal_bytes, int const vertical_rows) const {
  screen_overlay result;
  if(movable && (horizontal_bytes < -79 || horizontal_bytes > 79)) return result;
  auto const entry{movable ? entries[static_cast<unsigned int>(horizontal_bytes + 79)] : column_entry{}};
  int const remaining{movable ? 80 - std::abs(horizontal_bytes) : 80};
  int64_t const shift{movable ? horizontal_bytes + static_cast<int64_t>(vertical_rows) * (render::screen_width / 4) : 0};
  for(unsigned int index{entry.first}; index != pixels.size(); ++index) {
    auto const &pixel{pixels[index]};
    if(movable && static_cast<int>(pixel.column - entry.column) >= remaining) break;
    int64_t const address{pixel.address + shift};
    if(address < 0 || address >= render::screen_width * render::screen_height / 4) continue;
    for(unsigned int plane{0}; plane != 4; ++plane) if(pixel.planes & (1u << plane)) {
      auto const destination{static_cast<unsigned int>(address) * 4 + plane};
      result.image.write(destination,pixel.colour);
      result.coverage.pixels[destination] = 255;
    }
  }
  return result;
}

} // namespace tfx::resources
