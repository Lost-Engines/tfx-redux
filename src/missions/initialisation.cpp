#include "initialisation.h"
#include <bit>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace tfx::missions {
namespace {

std::string_view trim(std::string_view value) {
  auto const first{value.find_first_not_of(" \t\r\n")};
  if(first == value.npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
std::string lower(std::string_view const value) {
  std::string result{value};
  for(auto &c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return result;
}

class source_lines {
private:
  std::string_view remaining;

public:
  explicit source_lines(std::string_view const source) : remaining{source} {
  }
  std::optional<std::string_view> next() {
    if(remaining.empty()) return std::nullopt;
    auto const newline{remaining.find('\n')};
    auto const count{newline == remaining.npos ? remaining.size() : newline + 1};
    auto const result{remaining.substr(0, count)};
    remaining.remove_prefix(count);
    return result;
  }
  std::string text() {
    std::string result;
    while(auto const line{next()}) {
      auto const left{line->find_first_not_of(" \t")};
      auto const stripped{left == line->npos ? std::string_view{} : line->substr(left)};
      if(trim(stripped) == "END_TEXT") return result;
      result += stripped;
    }
    throw std::runtime_error{"mission text lacks END_TEXT"};
  }
};

class tokens {
private:
  std::string_view remaining;

public:
  explicit tokens(std::string_view const line) : remaining{line} {
  }
  std::string_view next() {
    auto const first{remaining.find_first_not_of(" \t\r\n,#()")};
    if(first == remaining.npos || remaining[first] == ';') {
      remaining = {};
      return {};
    }
    remaining.remove_prefix(first);
    auto const end{remaining.find_first_of(" \t\r\n,#()")};
    auto const count{end == remaining.npos ? remaining.size() : end};
    auto const result{remaining.substr(0, count)};
    remaining.remove_prefix(count);
    return result;
  }
  int32_t integer() {
    /// Native decimal conversion accepts a numeric prefix, including a leading plus
    auto word{next()};
    if(word.starts_with('+')) word.remove_prefix(1);
    if(word.empty()) return 0;
    int32_t value{0};
    auto const result{std::from_chars(word.data(), word.data() + word.size(), value)};
    if(result.ec == std::errc::result_out_of_range) throw std::runtime_error{"mission integer exceeds its native word"};
    return value;
  }
};

script_time scaled_time(int32_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value) * static_cast<uint32_t>(script_ticks_per_second));
}

std::optional<std::string> optional_text(tokens &fields, source_lines &source) {
  if(lower(fields.next()) == "text") return source.text();
  return std::nullopt;
}

} // anonymous namespace

setup initialise(std::string_view const script, inherited_setup const &inherited) {
  /// Translate the retail INIT declarations; FINISH expressions and resource/actor creation execute separately
  setup result{
    .start{
      .position{vec3<int32_t>{inherited.position}},
    },
    .speed_factor{inherited.speed_factor},
    .weapons{inherited.weapons},
  };
  source_lines source{script};
  bool initialising{false};
  while(auto const line{source.next()}) {
    tokens fields{*line};
    auto const command{lower(fields.next())};
    if(command.empty()) continue;
    if(!initialising) {
      if(command == "init:") initialising = true;
      continue;
    }
    if(command.ends_with(':')) break;
    if(command == "world") {
      result.world = fields.next();
    } else if(command == "type") {
      result.environment = fields.next();
    } else if(command == "way") {
      result.waypoints = fields.next();
    } else if(command == "start") {
      result.start.position.x = fields.integer();
      auto const altitude{fields.next()};
      result.start.landed = lower(altitude) == "landed";
      if(!result.start.landed) {
        tokens value{altitude};
        result.start.position.y = value.integer();
      }
      result.start.position.z = fields.integer();
    } else if(command == "plane") {
      auto const plane{fields.integer()};
      if(plane < 1 || plane > 3) throw std::runtime_error{"mission plane must select one of the three player craft"};
      result.aircraft = static_cast<world::player_aircraft>(plane);
    } else if(command == "angles") {
      result.angles.x = static_cast<math::angle>(fields.integer());
      result.angles.y = static_cast<math::angle>(fields.integer());
      result.angles.z = static_cast<math::angle>(fields.integer());
    } else if(command == "speed") {
      // original 0x8c414 converts a signed word with three float constants, then stores a float
      // preserve their exact values without assigning unverified physical units
      float constexpr fixed_speed_scale{1.0f / 256};
      float constexpr native_speed_scale{122};
      float constexpr reference_speed_reciprocal{0.0029411765281111f};
      int16_t const speed{math::signed_word(static_cast<uint16_t>(fields.integer()))};
      result.speed_factor = static_cast<float>(speed * static_cast<double>(fixed_speed_scale) * native_speed_scale * reference_speed_reciprocal);
    } else if(command == "weapons") {
      result.weapons = static_cast<uint8_t>(fields.integer());
    } else if(command == "default_weapons") {
      result.default_weapons = true;
    } else if(command == "setidnum") {
      result.aircraft_type_filter = math::signed_word(static_cast<uint16_t>(fields.integer()));
    } else if(command == "setmobileid") {
      result.surface_type_filter = math::signed_word(static_cast<uint16_t>(fields.integer()));
    } else if(command == "timer") {
      result.timer = fields.integer();
    } else if(command == "unc_start") {
      auto const extension{fields.next()};
      result.saved_start_extension = extension.starts_with('.') && extension.size() < 5 ? std::string{extension} : ".udd";
    } else if(command == "settime") {
      uint32_t const hours{static_cast<uint32_t>(fields.integer())};
      uint32_t const minutes{static_cast<uint32_t>(fields.integer())};
      uint32_t const seconds{static_cast<uint32_t>(fields.integer())};
      result.clock = std::bit_cast<int32_t>(((hours * 60 + minutes) * 60 + seconds) * static_cast<uint32_t>(script_ticks_per_second));
    } else if(command == "targ") {
      auto const number{fields.integer()};
      if(result.targets.size() >= maximum_targets || number != static_cast<int32_t>(result.targets.size()) + 1) throw std::runtime_error{"mission targets must be consecutively numbered within the native limit"};
      int32_t const x{fields.integer()};
      int32_t const z{fields.integer()};
      auto const centred{[](int32_t const cell){
        return std::bit_cast<int32_t>(static_cast<uint32_t>(cell) * world::cell_units + world::cell_units / 2);
      }};
      result.targets.push_back({
        .position{centred(x), 0, centred(z)},
      });
      auto const next{lower(fields.next())};
      if(next == "time") {
        result.targets.back().time_limit = scaled_time(fields.integer());
        result.targets.back().text = optional_text(fields, source);
      } else if(next == "text") result.targets.back().text = source.text();
    } else if(command == "flak") {
      if(result.flak.size() == maximum_flak_regions) throw std::runtime_error{"mission exceeds native flak region limit"};
      int32_t const x1{fields.integer()};
      int32_t const z1{fields.integer()};
      int32_t const x2{fields.integer()};
      int32_t const z2{fields.integer()};
      result.flak.push_back({
        .first{x1, z1},
        .last{x2, z2},
      });
    } else if(command == "message") {
      if(result.messages.size() == maximum_messages) throw std::runtime_error{"mission exceeds native timed message limit"};
      auto const time{scaled_time(fields.integer())};
      result.messages.push_back({
        .time{time},
        .text{source.text()},
      });
    } else if(command == "complete") {
      auto const kind{lower(fields.next())};
      completion_kind type;
      int32_t value{0};
      if(kind == "all_targets") {
        type = completion_kind::all_targets;
      } else {
        if(kind == "enemy") {
          type = completion_kind::enemy;
        } else if(kind == "allied") {
          type = completion_kind::allied;
        } else if(kind == "timer") {
          type = completion_kind::timer;
        } else if(kind == "emobile") {
          type = completion_kind::enemy_mobile;
        } else if(kind == "protect") {
          type = completion_kind::protect;
        } else {
          throw std::runtime_error{"unknown mission completion kind: " + kind};
        }
        fields.next();                                                         // original consumes the comparison token without interpreting it
        value = fields.integer();
        if(type == completion_kind::timer || type == completion_kind::protect) value = scaled_time(value);
      }
      result.completion = completion_rule{
        .kind{type},
        .value{value},
        .text{optional_text(fields, source)},
      };
    } else if(command == "setbob") {
      if(result.scenery_events.size() == maximum_scenery_events) throw std::runtime_error{"mission exceeds native scenery event limit"};
      auto const first{fields.next()};
      bool const timed{lower(first) == "time"};
      tokens value{first};
      auto const time{timed ? scaled_time(fields.integer()) : 0};
      auto const a{static_cast<uint8_t>(timed ? fields.integer() : value.integer())};
      auto const b{static_cast<uint8_t>(fields.integer())};
      auto const c{static_cast<uint8_t>(fields.integer())};
      result.scenery_events.push_back({
        .time{time},
        .entry{a},
        .cell{b, c},
      });
    } else {
      throw std::runtime_error{"unsupported mission INIT command: " + command};
    }
  }
  if(!initialising || result.world.empty() || result.environment.empty() || result.waypoints.empty()) throw std::runtime_error{"mission INIT lacks required world/type/way declarations"};
  return result;
}

} // namespace tfx::missions
