#include "settings.h"
#include <array>
#include <bit>
#include <charconv>
#include <fstream>
#include <optional>
#include <string>

namespace tfx::configuration {
namespace {


bool space(char const value) { return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\v' || value == '\f'; }
std::string_view skip_space(std::string_view value) {
  while(!value.empty() && space(value.front())) value.remove_prefix(1);
  return value;
}
std::optional<int32_t> scan_integer(std::string_view &value) {
  value = skip_space(value);
  bool const negative{!value.empty() && value.front() == '-'};
  if(!value.empty() && (value.front() == '-' || value.front() == '+')) value.remove_prefix(1);
  if(value.empty() || value.front() < '0' || value.front() > '9') return {};
  uint32_t number{0};
  while(!value.empty() && value.front() >= '0' && value.front() <= '9') {
    number = number * 10u + static_cast<unsigned int>(value.front() - '0');
    value.remove_prefix(1);
  }
  return std::bit_cast<int32_t>(negative ? 0u - number : number);
}
int32_t integer(std::string_view value) { return scan_integer(value).value_or(0); }
settings fallback() {
  settings state;
  state.detail = 0;
  state.language = 1;
  return state;
}

} // anonymous namespace

void apply(settings &state, std::string_view const key, std::string_view value) {
  if(value.empty()) return;
  auto const byte{static_cast<uint8_t>(integer(value))};
  if(key == "CONTROL") state.preferred_control = state.control = static_cast<flight::control_source>(byte);
  else if(key == "DETAIL") state.detail = byte;
  else if(key == "MODEL") state.model = simulation::configured_flight_model(static_cast<uint32_t>(integer(value)));
  else if(key == "SOUNDFX") state.sound_effects = byte;
  else if(key == "MUSIC") state.music = byte;
  else if(key == "STICK") state.stick_type = byte;
  else if(key == "LANG") state.language = byte;
  else if(key == "LOWMEM") state.low_memory = byte;
  else if(key == "IRQBASE") state.irq_base = integer(value);
  else if(key == "HUDCOL") state.hud_green = static_cast<uint16_t>(integer(value));
  else if(key == "JOYSCALE") {
    value = skip_space(value);
    if(!value.empty() && value.front() == '+') value.remove_prefix(1);
    float scale{0};
    auto const [end,error]{std::from_chars(value.data(),value.data() + value.size(),scale)};
    if(error == std::errc{} && end != value.data()) state.joystick_scale = scale == 0 ? 0.0f : scale; // Native scanf discards the sign of zero.
  } else if(key == "REALISM") {
    std::array<uint8_t,7> values{};
    for(auto &item : values) {
      auto const number{scan_integer(value)};
      if(!number) break;
      item = static_cast<uint8_t>(*number);
      if(value.empty() || value.front() != ',') break;
      value.remove_prefix(1);
    }
    state.effects = {
      .g_force{static_cast<g_effects>(values[0])},
      .precipitation{values[1]},
      .crashing{values[2]},
      .gear_damage{values[3]},
      .wind{values[4]},
      .temperature{values[5]},
      .recovery{values[6]},
    };
  }
}

settings parse(std::string_view const text) {
  if(text.empty() || text.size() >= configuration_buffer_size) return fallback();
  settings state;
  // The native reader removes CR characters before splitting LF lines.
  std::string normal;
  normal.reserve(text.size());
  for(char const character : text) {
    if(character == '\0') break;
    if(character != '\r') normal.push_back(character);
  }
  std::string_view remaining{normal};
  while(!remaining.empty()) {
    auto const end{remaining.find('\n')};
    auto const line{remaining.substr(0,end)};
    auto const equals{line.find('=')};
    if(equals != std::string_view::npos) apply(state,line.substr(0,equals),line.substr(equals + 1));
    if(end == std::string_view::npos) break;
    remaining.remove_prefix(end + 1);
  }
  if(state.sound_effects == 1 && state.music == 0) state.music = 1; // Native coupled SB driver selection.
  return state;
}

settings load(std::filesystem::path const &directory) {
  auto path{directory / "TFX.CFG"};
  if(!std::filesystem::exists(path)) path = directory / "tfx.cfg";
  std::ifstream stream{path,std::ios::binary};
  if(!stream) {
    // A fresh native port starts with keyboard flight. Arcade has no keyboard
    // steering law, so pair this with Simple; existing DOS files still parse
    // with their original defaults, including an omitted MODEL.
    auto state{fallback()};
    state.model = simulation::flight_model::simple;
    return state;
  }
  std::string text(configuration_buffer_size,'\0');
  stream.read(text.data(),text.size());
  auto const count{stream.gcount()};
  if(count == 0 || count >= configuration_buffer_size) return fallback();
  text.resize(static_cast<unsigned int>(count));
  return parse(text);
}

} // namespace tfx::configuration
