#include "document.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tfx::planning {
namespace {
std::optional<int32_t> take_number(std::string_view &text) {
  auto const at{text.find_first_not_of(" \t")};
  if(at == std::string_view::npos) return {};
  text.remove_prefix(at);
  int32_t value{0};
  auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
  if(error != std::errc{}) return {};
  text.remove_prefix(static_cast<unsigned int>(end - text.data()));
  return value;
}
class lines {
private:
  std::string_view source;

public:
  explicit lines(std::string_view const source) : source{source} {
  }
  std::string next() {
    if(source.empty()) throw std::runtime_error{"planner document ended before its route terminator"};
    auto const newline{source.find('\n')};
    std::string result{source.substr(0, newline)};
    source.remove_prefix(newline == std::string_view::npos ? source.size() : newline + 1);
    std::erase(result, '\r');                                                  // Native 0xa7c88 ignores carriage returns throughout each line.
    return result;
  }
  std::string rest() const {
    return std::string{source};
  }
};
std::vector<int32_t> header_words(std::string_view header) {
  std::vector<int32_t> words;
  while(words.size() != 16) {
    auto const value{take_number(header)};
    if(!value) break;
    words.push_back(*value);
  }
  return words;
}
} // anonymous namespace

mission_parameters::mission_parameters(std::span<int32_t const> const source) {
  auto values{words()};
  if(source.size() >= values.size()) throw std::runtime_error{"planner header exceeds native parameter storage"};
  // Native scanf writes dwords through fifteen pointers spaced only two bytes apart.
  // Each following field overwrites the previous high half; a partial header retains its final spill word.
  for(unsigned int index{0}; index != source.size(); ++index) {
    auto const bits{std::bit_cast<uint32_t>(source[index])};
    values[index] = static_cast<uint16_t>(bits);
    values[index + 1] = static_cast<uint16_t>(bits >> 16);
  }
  aircraft_index = std::bit_cast<int16_t>(values[0]);
  departure_cell = values[1];
  primary_target_cell = values[2];
  additional_target_count = values[3];
  std::ranges::copy(std::span{values}.subspan(4, additional_targets.size()), additional_targets.begin());
  weather_index = std::bit_cast<int16_t>(values[11]);
  std::ranges::copy(std::span{values}.subspan(12, retained.size()), retained.begin());
}
std::array<uint16_t, 16> mission_parameters::words() const {
  std::array<uint16_t, 16> result{
    std::bit_cast<uint16_t>(aircraft_index), departure_cell, primary_target_cell, additional_target_count,
  };
  std::ranges::copy(additional_targets, result.begin() + 4);
  result[11] = std::bit_cast<uint16_t>(weather_index);
  std::ranges::copy(retained, result.begin() + 12);
  return result;
}
document::document(std::string_view const source) : parameters{std::span<int32_t const>{}} {
  lines input{source};
  auto const header{header_words(input.next())};
  if(header.empty()) throw std::runtime_error{"planner document has no format word"};
  format = static_cast<uint16_t>(header.front());
  parameters = mission_parameters{std::span{header}.subspan(1)};
  map_images[0] = input.next();
  world = input.next();
  map_images[1] = input.next();
  palette = input.next();
  map_images[2] = input.next();
  auto const count_line{input.next()};
  std::string_view count_view{count_line};
  auto const count{take_number(count_view)};
  if(!count || *count < 0 || *count > static_cast<int32_t>(maximum_routes)) throw std::runtime_error{"planner route count exceeds native storage"};
  for(unsigned int index{0}; index != static_cast<unsigned int>(*count); ++index) {
    route path{
      .name{input.next().substr(0, route_name_bytes)},
      .points{},
    };
    for(;;) {
      auto const line{input.next()};
      if(line == "=ENDPATH=") break;
      if(!line.starts_with("WP")) throw std::runtime_error{"planner route has no waypoint header"};
      std::string_view fields{line};
      fields.remove_prefix(2);
      auto const ordinal{take_number(fields)};
      auto const x{take_number(fields)};
      auto const z{take_number(fields)};
      if(!ordinal || !x || !z) throw std::runtime_error{"planner waypoint has incomplete coordinates"};
      waypoint point{
        .ordinal{std::bit_cast<int16_t>(static_cast<uint16_t>(*ordinal))},
        .cell{std::bit_cast<int16_t>(static_cast<uint16_t>(*x)), std::bit_cast<int16_t>(static_cast<uint16_t>(*z))},
        .commands{},
      };
      for(;;) {
        auto command{input.next()};
        if(command == "-endcmnd-") break;
        point.commands.push_back(std::move(command));
      }
      path.points.push_back(std::move(point));
    }
    routes.push_back(std::move(path));
  }
  notes = input.rest();
}

} // namespace tfx::planning
