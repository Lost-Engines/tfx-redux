#include "definition.h"
#include <bit>
#include <stdexcept>

namespace tfx::weapons {
namespace {

flight_stage callback(uint32_t const address) {
  switch(address) {
  case 0:
    return flight_stage::finished;
  case 0xa4380:
    return flight_stage::attached;
  case 0xa4414:
    return flight_stage::falling;
  case 0xa422c:
    return flight_stage::accelerating;
  case 0xa42bc:
    return flight_stage::accelerating_alias;
  case 0xa42c8:
    return flight_stage::coasting;
  case 0xa3b6c:
    return flight_stage::guided;
  case 0xa3d70:
    return flight_stage::sea_skimming;
  case 0xa3f14:
    return flight_stage::retarded_store;
  case 0xa3fd8:
    return flight_stage::falling_store;
  case 0xa4080:
    return flight_stage::attached_dispenser;
  case 0xa41a0:
    return flight_stage::falling_dispenser;
  default:
    throw std::runtime_error{"unknown original projectile callback"};
  }
}

} // anonymous namespace

std::vector<definition> load_definitions(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr data_file{973476};
  unsigned int constexpr records{data_file + 0x809c};
  unsigned int constexpr record_bytes{48};
  unsigned int constexpr code_file{0x70aa4};
  auto const name{[&](uint32_t const address){
    std::string text;
    for(unsigned int index{0}; index != 32; ++index) {
      auto const byte{source.byte(data_file + address + index)};
      if(!byte) return text;
      text.push_back(static_cast<char>(byte));
    }
    throw std::runtime_error{"unterminated weapon definition name"};
  }};
  std::vector<definition> result;
  for(unsigned int index{0}; index != weapon_count; ++index) {
    unsigned int const at{records + index * record_bytes};
    definition item{
      .label{name(source.le32(at + 4))},
      .carried{name(source.le32(at + 8)).substr(0, 8)},
      .fired{("x" + name(source.le32(at + 8))).substr(0, 8)},
      .secondary{name(source.le32(at + 12)).substr(0, 8)},
      .role{static_cast<weapon_role>(source.byte(at + 20))},
      .category{source.byte(at + 29)},
      .impact_effects{source.byte(at + 30) != 2},
      .mass{source.signed_le16(at + 31)},
      .mount_masks{source.le16(at + 23), source.le16(at + 27), source.le16(at + 25)},
      .range{std::bit_cast<int32_t>(source.le32(at + 35))},
      .minimum_range{source.signed_le16(at + 33)},
      .launch_speed{source.signed_le16(at + 41)},
      .player_proximity{source.byte(at + 43) != 0},
      .drag{std::bit_cast<float>(source.le32(at + 44))},
      .stages{},
    };
    unsigned int const stages{data_file + source.le32(at)};
    for(unsigned int ordinal{0}; ordinal != 32; ++ordinal) {
      unsigned int const step{stages + ordinal * 6};
      auto const ticks{source.le16(step)};
      auto const pointer{source.le32(step + 2)};
      item.stages.push_back({
        .ticks{ticks},
        .motion{callback(pointer ? pointer + code_file : 0)},
      });
      if(!ticks) break;
      if(ordinal == 31) throw std::runtime_error{"unterminated weapon stage sequence"};
    }
    result.push_back(std::move(item));
  }
  return result;
}

} // namespace tfx::weapons
