#include "menu.h"
#include <bit>
#include <charconv>
#include <map>
#include <stdexcept>

namespace tfx::presentation {
namespace {
std::string_view trim(std::string_view const source) {
  auto const begin{source.find_first_not_of(" \t\r\n")};
  if(begin == std::string_view::npos) return {};
  return source.substr(begin,source.find_last_not_of(" \t\r\n") - begin + 1);
}
int32_t number(std::string_view const source) {
  auto const text{trim(source)};
  int32_t result{0};
  auto const [end,error]{std::from_chars(text.data(),text.data() + text.size(),result)};
  if(error != std::errc{} || end != text.data() + text.size()) throw std::runtime_error{"invalid menu integer: " + std::string{text}};
  return result;
}
std::string lower(std::string_view const source) {
  std::string result{source};
  for(auto &letter : result) if(letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter + ('a' - 'A'));
  return result;
}
} // anonymous namespace

menu_program::menu_program(std::string_view const source) {
  // Native 0xd5618's shipped subset: declarative assets/click boxes, then named numeric choices.
  struct block { std::vector<menu_assignment> assignments; bool returned{false}; };
  std::map<std::string,block,std::less<>> labels;
  std::string label;
  bool displayed{false};
  unsigned int cursor{0};
  while(cursor < source.size()) {
    auto const next{source.find('\n',cursor)};
    auto line{source.substr(cursor,next == std::string_view::npos ? next : next - cursor)};
    cursor = next == std::string_view::npos ? static_cast<unsigned int>(source.size()) : static_cast<unsigned int>(next + 1);
    line = trim(line.substr(0,line.find_first_of(std::string_view{";\x1a",2})));
    if(line.empty()) continue;
    std::string const normal{lower(line)};
    if(normal == "eof") break;
    if(normal.ends_with(':')) {
      label = normal.substr(0,normal.size() - 1);
      if(!displayed || label.empty() || labels.contains(label)) throw std::runtime_error{"invalid or duplicate menu label"};
      labels.emplace(label,block{});
      continue;
    }
    if(!label.empty()) {
      auto &body{labels.at(label)};
      if(body.returned) throw std::runtime_error{"unreachable menu choice commands"};
      if(normal == "return") { body.returned = true;continue; }
      auto const equals{normal.find('=')};
      if(equals == std::string::npos) throw std::runtime_error{"unsupported menu choice command: " + normal};
      body.assignments.push_back({
        .variable{trim(std::string_view{normal}.substr(0,equals))},
        .value{std::bit_cast<uint32_t>(number(std::string_view{normal}.substr(equals + 1)))},
      });
      continue;
    }
    auto const split{normal.find_first_of(" \t=")};
    auto const command{normal.substr(0,split)};
    auto arguments{split == std::string::npos ? std::string_view{} : trim(std::string_view{normal}.substr(split))};
    if(displayed) throw std::runtime_error{"unsupported menu continuation after display"};
    if(command == "backdrop") { backdrop = arguments;animation.clear(); } else if(command == "anim") { animation = arguments;backdrop.clear(); } else if(command == "nobox") flash_box = false;
    else if(command == "mouse") {
      if(arguments.starts_with('=')) arguments = trim(arguments.substr(1));
      cursor_style = static_cast<uint16_t>(number(arguments));
    } else if(command == "display") displayed = true;                          // Its optional trailing name is not consumed by native display.
    else if(command == "clickbox") {
      std::array<int16_t,4> bounds;
      for(auto &bound : bounds) {
        auto const comma{arguments.find(',')};
        if(comma == std::string_view::npos) throw std::runtime_error{"menu click box needs four bounds and a label"};
        bound = math::signed_word(static_cast<uint16_t>(number(arguments.substr(0,comma))));
        arguments = trim(arguments.substr(comma + 1));
      }
      choices.push_back({
        .minimum{bounds[0],bounds[1]},
        .maximum{bounds[2],bounds[3]},
        .label{arguments},
        .assignments{},
      });
    } else throw std::runtime_error{"unsupported menu setup command: " + command};
  }
  if(!displayed || (backdrop.empty() && animation.empty())) throw std::runtime_error{"menu needs a display and background asset"};
  for(auto &choice : choices) {
    auto const found{labels.find(choice.label)};
    if(found == labels.end() || !found->second.returned) throw std::runtime_error{"menu choice has no returning label: " + choice.label};
    choice.assignments = found->second.assignments;
  }
}

std::optional<unsigned int> menu_program::hit_test(vec2<int16_t> const &position) const {
  // Native 0xd52d0 uses inclusive word bounds and the first matching declaration.
  for(unsigned int index{0}; index != choices.size(); ++index) {
    auto const &choice{choices[index]};
    if(position.x >= choice.minimum.x && position.x <= choice.maximum.x && position.y >= choice.minimum.y && position.y <= choice.maximum.y) return index;
  }
  return {};
}
void menu_program::choose(unsigned int const index, variables &state) const {
  for(auto const &assignment : choices.at(index).assignments) {
    auto const found{state.find(assignment.variable)};
    if(found == state.end() || !std::holds_alternative<uint32_t>(found->second)) throw std::runtime_error{"menu names an undefined numeric variable: " + assignment.variable};
    found->second = assignment.value;
  }
}

} // namespace tfx::presentation
