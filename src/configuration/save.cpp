#include "save.h"
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace tfx::configuration {
namespace {

void replace(std::string &text, std::string_view const keyword, std::string const &value) {
  // Original 0xa67c8 finds only the first case-sensitive prefix match. Its
  // line skipper consumes every adjacent CR/LF after the matching line.
  unsigned int start{0};
  while(start < text.size()) {
    auto end{text.find_first_of("\r\n", start)};
    if(end == std::string::npos) end = text.size();
    while(end < text.size() && (text[end] == '\r' || text[end] == '\n')) ++end;
    if(text.compare(start, keyword.size(), keyword) == 0) {
      text.erase(start, end - start);
      break;
    }
    start = static_cast<unsigned int>(end);
  }
  // Appending follows each removal, rather than removing all four at once.
  // A source file lacking its final newline retains that original quirk.
  text += value;
}

} // anonymous namespace

std::optional<std::string> update_preferences(std::string_view const original, settings const &state) {
  if(original.empty() || original.size() >= configuration_buffer_size) return {};
  std::string text{original.substr(0, original.find('\0'))};
  auto const &r{state.effects};
  replace(text, "REALISM", "REALISM=" + std::to_string(std::to_underlying(r.g_force)) + ',' + std::to_string(r.precipitation) + ',' + std::to_string(r.crashing) + ',' + std::to_string(r.gear_damage) + ',' + std::to_string(r.wind) + ',' + std::to_string(r.temperature) + ',' + std::to_string(r.recovery) + "\r\n");
  replace(text, "DETAIL", "DETAIL=" + std::to_string(state.detail) + "\r\n");
  std::array<char, 64> scale{};
  int const length{std::snprintf(scale.data(), scale.size(), "JOYSCALE=%3.0f\r\n", static_cast<double>(state.joystick_scale == 0 ? 0.0f : state.joystick_scale))};
  if(length < 0 || static_cast<unsigned int>(length) >= scale.size()) throw std::runtime_error{"cannot format joystick sensitivity"};
  replace(text, "JOYSCALE", {scale.data(), static_cast<unsigned int>(length)});
  replace(text, "HUDCOL", "HUDCOL=" + std::to_string(std::bit_cast<int16_t>(state.hud_green)) + "\r\n");
  if(text.size() >= configuration_buffer_size) return {};
  return text;
}

bool save_preferences(std::filesystem::path const &directory, settings const &state) {
  auto path{directory / "TFX.CFG"};
  if(!std::filesystem::exists(path)) path = directory / "tfx.cfg";
  std::ifstream input{path, std::ios::binary};
  if(!input) return false;
  std::string original(configuration_buffer_size, '\0');
  input.read(original.data(), original.size());
  original.resize(static_cast<unsigned int>(input.gcount()));
  auto const updated{update_preferences(original, state)};
  if(!updated) return false;
  input.close();
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(updated->data(), static_cast<std::streamsize>(updated->size()));
  if(!output) throw std::runtime_error{"cannot save flight preferences to " + path.string()};
  return true;
}

} // namespace tfx::configuration
