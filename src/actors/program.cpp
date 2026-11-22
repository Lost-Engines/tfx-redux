#include "program.h"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
#include <utility>
#include "world/environment.h"

namespace tfx::actors {
namespace {

unsigned int command_size(uint8_t const opcode) {
  static std::array<unsigned int,17> constexpr sizes{1,9,7,5,7,17,43,11,9,7,19,3,25,3,25,0,13};
  if(opcode >= sizes.size() || !sizes[opcode]) throw std::runtime_error{"unknown compiled actor command " + std::to_string(opcode)};
  return sizes[opcode];
}
std::string string(resources::binary_view const &view, unsigned int const at, unsigned int const length) {
  std::string result;
  for(auto const value : view.slice(at,length)) {
    if(!value) break;
    result += static_cast<char>(value);
  }
  return result;
}
int32_t wrap(int64_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value));
}
int32_t cell_position(int16_t const cell) {
  return wrap(static_cast<int64_t>(cell) * world::cell_units * position_unit + world::cell_units / 2 * position_unit);
}

} // anonymous namespace

program::program(std::span<uint8_t const> const source) {
  resources::binary_view const view{source};
  unsigned int cursor{0};
  while(true) {
    auto const size{view.le16(cursor)};
    cursor += 2;
    if(!size) {
      if(cursor != source.size()) throw std::runtime_error{"data follows compiled actor terminator"};
      return;
    }
    auto const payload{view.slice(cursor,size)};
    auto const type{view.byte(cursor)};
    if(type < 14 || type > 18) throw std::runtime_error{"unknown compiled actor record type"};
    actor_record record{
      .kind{static_cast<record_kind>(type)},
      .source_offset{cursor},
      .body{payload.begin(),payload.end()},
      .commands{},
    };
    resources::binary_view const body{payload};
    if(record.kind == record_kind::aircraft || record.kind == record_kind::surface) {
      for(unsigned int at{0}; at < size;) {
        auto const opcode{body.byte(at)};
        auto const length{command_size(opcode)};
        auto const operands{body.slice(at + 1,length - 1)};
        record.commands.push_back({
          .address{at},
          .opcode{static_cast<command>(opcode)},
          .operands{operands.begin(),operands.end()},
        });
        at += length;
      }
    } else if(record.kind == record_kind::player_route) {
      auto const count{body.le16(1)};
      if(size != 3u + count * 2u) throw std::runtime_error{"player route count differs from its record extent"};
      player_route.clear();
      for(unsigned int i{0}; i != count; ++i) player_route.emplace_back(body.byte(3 + i * 2) * world::cell_units + world::cell_units / 2,0,body.byte(4 + i * 2) * world::cell_units + world::cell_units / 2);
    } else if(record.kind == record_kind::scenery && size < 21) throw std::runtime_error{"truncated fixed scenery actor"};
    records.push_back(std::move(record));
    cursor += size;
  }
}

std::vector<profile> profiles(std::span<uint8_t const> const source) {
  resources::binary_view const view{source};
  auto const count{view.le16(2)};
  unsigned int at{4};
  std::vector<profile> result;
  while(at < source.size()) {
    auto const type{static_cast<unsigned int>(view.byte(at) & 127)};
    if(type < 14 || type > 17) break;
    static std::array<unsigned int,4> constexpr record_bytes{air_profile_bytes,58,46,34};
    unsigned int const size{record_bytes[type - 14]};
    auto const bytes{view.slice(at,size)};
    auto definition{string(view,at + 21,10)};
    std::ranges::transform(definition,definition.begin(),[](unsigned char const c){ return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    result.push_back({
      .id{view.le16(at + 32)},
      .kind{static_cast<profile_kind>(type)},
      .side{static_cast<allegiance>(view.byte(at + 31))},
      .name{string(view,at + 1,20)},
      .definition{std::move(definition)},
      .source{bytes.begin(),bytes.end()},
      // The native loader keeps contiguous records. Some shipped air groups use a surface profile and read into its successor.
      .native_window{source.begin() + at,source.begin() + at + std::min<unsigned int>(air_profile_bytes,static_cast<unsigned int>(source.size() - at))},
    });
    at += size;
  }
  if(result.size() != count || std::ranges::any_of(source.subspan(at),[](uint8_t const value){ return value != 0; })) throw std::runtime_error{"actor profile count or terminator differs"};
  return result;
}

void move(vec3<int32_t> &position, vec3<math::angle> const &angles, int32_t const distance, int const minimum_y, int const maximum_y) {
  math::advance_position(position,angles,distance,minimum_y * position_unit,maximum_y * position_unit);
}

std::vector<initial_actor> initialise(program const &source, std::span<profile const> const profiles, std::span<formation_offset const> const offsets) {
  /// Native 0x75bc8 / 0x7b9b0: independent 50-slot allied/other air pools and one 50-slot surface pool
  if(offsets.size() < aircraft_per_group) throw std::invalid_argument{"missing native formation offsets"};
  uint16_t constexpr initial_air_flags{0xfc40}, airborne_flag{1}, awaiting_takeoff_flag{0x10};
  unsigned int constexpr angle_per_degree{182};                                // native integer approximation, not a recomputed 65536/360 conversion
  std::vector<initial_actor> result;
  unsigned int allied{0}, other{0}, surface{0};
  for(unsigned int index{0}; index != source.records.size(); ++index) {
    auto const &record{source.records[index]};
    if(record.kind != record_kind::aircraft && record.kind != record_kind::surface) continue;
    bool const air{record.kind == record_kind::aircraft};
    resources::binary_view const body{record.body};
    auto const id{body.le16(air ? 7 : 5)};
    auto const profile{std::ranges::find(profiles,id,&actors::profile::id)};
    if(profile == profiles.end()) throw std::runtime_error{"compiled actor references an unknown profile " + std::to_string(id)};
    auto &slot{air ? (profile->side == allegiance::allied ? allied : other) : surface};
    if(slot == (air ? aircraft_per_side : surface_capacity)) continue;
    initial_actor actor{
      .kind{record.kind},
      .record{index},
      .profile_id{id},
      .slot{slot++},
      .side{profile->side},
      .position{cell_position(body.signed_le16(air ? 3 : 1)),0,cell_position(body.signed_le16(air ? 5 : 3))},
      .angles{0,0,0},
      .speed{0},
      .flags{static_cast<uint16_t>(air ? initial_air_flags | (profile->side == allegiance::allied ? 0u : 8u) : 4u | (profile->side == allegiance::allied ? 0u : 2u))},
    };
    bool const airborne{air && body.le16(9) != 0};
    if(air && !airborne) {
      actor.position.y = -2 * position_unit;
      actor.flags |= awaiting_takeoff_flag;
      actor.angles.y = static_cast<math::angle>(body.le16(19) * angle_per_degree);
    } else {
      actor.position.y = -static_cast<int32_t>(body.le16(air ? 11 : 7)) * 32;
      actor.speed = std::bit_cast<int32_t>(static_cast<uint32_t>(body.le16(air ? 13 : 11)) << speed_fraction_bits);
      if(air) actor.flags |= airborne_flag;
      else actor.angles.y = static_cast<math::angle>(body.le16(9) * angle_per_degree);
    }
    result.push_back(actor);
    if(!air) continue;
    int const count{std::min(static_cast<int>(body.signed_le16(1)),static_cast<int>(aircraft_per_group))};
    for(int member{1}; member < count && slot < aircraft_per_side; ++member) {
      // The original copies the previous wingman, so successive offsets accumulate from that member.
      auto wing{result.back()};
      wing.slot = slot++;
      wing.leader = static_cast<uint8_t>(actor.slot);
      wing.formation = static_cast<uint8_t>(member);
      auto const &offset{offsets[static_cast<unsigned int>(member)]};
      if(airborne) wing.position.y = wrap(static_cast<int64_t>(wing.position.y) + offset.height);
      vec3<math::angle> const placement_angles{0,static_cast<math::angle>(wing.angles.y + offset.heading),0};
      move(wing.position,placement_angles,offset.distance);
      result.push_back(std::move(wing));
    }
  }
  return result;
}

} // namespace tfx::actors
