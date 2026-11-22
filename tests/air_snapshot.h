#pragma once

#include <ostream>
#include <span>
#include <type_traits>
#include <utility>
#include "actors/air.h"

namespace tfx::test {

inline uint64_t air_snapshot(actors::program const &program, actors::air_runtime const &state, std::span<actors::air_request const> const requests, std::ostream *const output = nullptr, bool const combat = false) {
  uint64_t digest{14695981039346656037ULL};
  auto const word{[&](auto const value){
    if(output) *output << static_cast<int64_t>(value) << ' ';
    for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
  }};
  for(unsigned int slot{0}; slot != state.aircraft().size(); ++slot) {
    auto const &aircraft{state.aircraft()[slot]};
    if(!aircraft) continue;
    auto const &flight{aircraft->flight};
    auto const &pose{flight.pose};
    word(slot);
    for(auto const value : {pose.position.x,pose.position.y,pose.position.z}) word(value);
    for(auto const value : {pose.angles.x,pose.angles.y,pose.angles.z}) word(value);
    word(pose.speed);
    for(auto const value : {flight.angular_rate.x,flight.angular_rate.y,flight.angular_rate.z}) word(value);
    word(flight.acceleration); word(flight.input_ticks); word(aircraft->wait_seconds); word(aircraft->actions);
    word(program.records.at(aircraft->location.record).source_offset + aircraft->location.address);
    word(pose.flags); word(pose.leader); word(pose.formation);
    if(combat) {
      word(aircraft->target ? static_cast<int>(*aircraft->target) : -1);
      word(aircraft->manoeuvre ? static_cast<int>(aircraft->manoeuvre->program) : -1);
      word(aircraft->manoeuvre ? static_cast<int>(aircraft->manoeuvre->address) : -1);
      word(aircraft->scan_cursor); word(aircraft->player_shots);
    }
  }
  for(unsigned int record{0}; record != program.records.size(); ++record) for(auto const value : state.program_bytes(record)) word(value);
  word(requests.size());
  for(auto const &request : requests) {
    word(request.actor); word(request.data.index());
    std::visit([&](auto const &data){
      using data_type = std::remove_cvref_t<decltype(data)>;
      if constexpr(std::is_same_v<data_type,tfx::actors::air_scenery_event>) word(data.entry);
      else if constexpr(std::is_same_v<data_type,tfx::actors::air_radio>) {
        word(data.text.size());
        for(unsigned char const byte : data.text) word(byte);
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_weapon>) {
        for(auto const value : {data.position.x,data.position.y,data.position.z}) word(value);
        for(auto const value : {data.angles.x,data.angles.y,data.angles.z}) word(value);
        word(data.speed); word(data.weapon);
        for(auto const value : {data.target.x,data.target.y,data.target.z}) word(value);
        if(combat) word(data.target_actor ? static_cast<int>(*data.target_actor) : -1);
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_impact>) {
        for(auto const value : {data.position.x,data.position.y,data.position.z}) word(value);
        word(data.kind);
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_warning>) {
        word(data.target); word(std::to_underlying(data.kind));
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_countermeasure>) {
        word(std::to_underlying(data.kind)); word(data.strength);
      }
    },request.data);
  }
  return digest;
}

} // namespace tfx::test
