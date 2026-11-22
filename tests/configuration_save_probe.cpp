#include <bit>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include "configuration/save.h"

namespace {
std::string decode(std::string_view const hex) {
  if(hex == "-") return {};
  if(hex.size() % 2) throw std::runtime_error{"incomplete hex byte"};
  std::string result;
  for(unsigned int index{0}; index < hex.size(); index += 2) {
    unsigned int value{0};
    auto const [end,error]{std::from_chars(hex.data() + index,hex.data() + index + 2,value,16)};
    if(error != std::errc{} || end != hex.data() + index + 2) throw std::runtime_error{"invalid hex byte"};
    result.push_back(static_cast<char>(value));
  }
  return result;
}
std::string encode(std::optional<std::string> const &text) {
  if(!text) return "-";
  std::string result;
  std::string_view constexpr digits{"0123456789abcdef"};
  for(unsigned char const value : *text) { result += digits[value >> 4]; result += digits[value & 15]; }
  return result;
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  std::string line;
  while(std::getline(input,line)) {
    auto const first{line.find('|')}, last{line.find('|',first + 1)};
    if(first == std::string::npos) throw std::runtime_error{"missing preference separator"};
    tfx::configuration::settings state;
    unsigned int detail,scale,green,g,precipitation,crashing,gear,wind,temperature,recovery;
    std::istringstream fields{line.substr(0,first)};
    if(!(fields >> detail >> scale >> green >> g >> precipitation >> crashing >> gear >> wind >> temperature >> recovery)) throw std::runtime_error{"missing preference fields"};
    state.detail = static_cast<uint8_t>(detail);
    state.joystick_scale = std::bit_cast<float>(scale);
    state.hud_green = static_cast<uint16_t>(green);
    state.effects = {
      .g_force{static_cast<tfx::configuration::g_effects>(g)},
      .precipitation{static_cast<uint8_t>(precipitation)},
      .crashing{static_cast<uint8_t>(crashing)},
      .gear_damage{static_cast<uint8_t>(gear)},
      .wind{static_cast<uint8_t>(wind)},
      .temperature{static_cast<uint8_t>(temperature)},
      .recovery{static_cast<uint8_t>(recovery)},
    };
    auto const actual{encode(tfx::configuration::update_preferences(decode(std::string_view{line}.substr(first + 1,last == std::string::npos ? last : last - first - 1)),state))};
    if(argc == 2) { if(last == std::string::npos || line.substr(last + 1) != actual) throw std::runtime_error{"preference mismatch at case " + std::to_string(cases)}; } else std::cout << actual << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
