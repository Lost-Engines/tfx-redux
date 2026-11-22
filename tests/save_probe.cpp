#include <cstdlib>
#include <iostream>
#include "presentation/save.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"save_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::presentation::program const program{game.read("tfx.par")};
  unsigned int mode{0};
  while(std::cin >> mode) {
    tfx::presentation::saved_state state;
    if(mode == 0) {
      unsigned int length{0};
      unsigned int byte{0};
      std::cin >> length;
      tfx::resources::bytes input;
      for(unsigned int index{0}; index != length; ++index) {
        std::cin >> byte;
        input.push_back(static_cast<uint8_t>(byte));
      }
      state = tfx::presentation::decode_save(program, input);
    } else {
      for(auto const &variable : program.variables) {
        if(variable.text) {
          std::string text;
          std::cin >> text;
          state.values[variable.name] = text == "-" ? std::string{} : text;
        } else {
          uint32_t value{0};
          std::cin >> value;
          state.values[variable.name] = value;
        }
      }
      for(unsigned int index{static_cast<unsigned int>(program.variables.size())}; index != tfx::presentation::saved_variable_slots; ++index) {
        uint32_t value{0};
        std::cin >> value;
        state.reserved.push_back(value);
      }
      unsigned int count{0};
      std::cin >> count;
      for(unsigned int index{0}; index != count; ++index) {
        tfx::presentation::location position;
        std::cin >> position.function >> position.statement;
        state.callers.push_back(position);
      }
      int function{0};
      int statement{0};
      std::cin >> function >> statement;
      if(function >= 0) state.resume = tfx::presentation::location{static_cast<unsigned int>(function), static_cast<unsigned int>(statement)};
    }
    auto const output{tfx::presentation::encode_save(program, state)};
    auto const roundtrip{tfx::presentation::decode_save(program, output)};
    if(roundtrip.values != state.values || roundtrip.reserved != state.reserved || roundtrip.callers != state.callers || roundtrip.resume != state.resume) throw std::runtime_error{"save roundtrip changed logical state"};
    std::cout << output.size();
    for(auto const byte : output) std::cout << ' ' << static_cast<unsigned int>(byte);
    std::cout << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
