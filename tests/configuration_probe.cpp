#include <bit>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include "configuration/settings.h"

int main(int const argc, char const *const argv[]) try {
  bool const document{argc >= 2 && std::string_view{argv[1]} == "--document"};
  int const file_argument{document ? 2 : 1};
  bool const check{argc > file_argument};
  std::ifstream fixture;
  if(check) fixture.open(argv[file_argument]);
  if(argc > file_argument + 1 || (check && !fixture)) throw std::runtime_error{"expected [--document] [FILE]"};
  auto &input{check ? fixture : std::cin};
  unsigned int cases{0};
  std::string line;
  while(std::getline(input, line)) {
    auto const separator{line.find('|')};
    auto second{line.find('|', separator + 1)};
    if(separator == std::string::npos) throw std::runtime_error{"missing configuration fixture separator"};
    tfx::configuration::settings state;
    if(document) {
      std::string text;
      auto const hex{line.substr(0, separator)};
      if(hex != "-") {
        for(unsigned int index{0}; index < hex.size(); index += 2) {
          unsigned int byte{0};
          auto const [end, error]{std::from_chars(hex.data() + index, hex.data() + index + 2, byte, 16)};
          if(error != std::errc{} || end != hex.data() + index + 2) throw std::runtime_error{"invalid configuration text encoding"};
          text.push_back(static_cast<char>(byte));
        }
      }
      state = tfx::configuration::parse(text);
      second = separator;
    } else {
      tfx::configuration::apply(state, line.substr(0, separator), line.substr(separator + 1, second == std::string::npos ? second : second - separator - 1));
    }
    auto const &r{state.effects};
    std::array<int64_t, 18> const values{
      static_cast<int64_t>(state.control), state.detail, static_cast<int64_t>(state.model), state.sound_effects, state.music, state.stick_type, state.language, state.low_memory, state.irq_base,
      static_cast<int64_t>(r.g_force), r.precipitation, r.crashing, r.gear_damage, r.wind, r.temperature, r.recovery,
      std::bit_cast<uint32_t>(state.joystick_scale), state.hud_green,
    };
    if(check) {
      if(second == std::string::npos) throw std::runtime_error{"missing expected configuration values"};
      std::istringstream expected{line.substr(second + 1)};
      for(auto const value : values) {
        int64_t want;
        if(!(expected >> want) || want != value) throw std::runtime_error{"configuration mismatch in case " + std::to_string(cases)};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
