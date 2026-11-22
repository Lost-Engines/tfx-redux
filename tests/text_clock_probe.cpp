#include <cstdlib>
#include <fstream>
#include <iostream>
#include "presentation/text.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read text-clock fixture"};
  unsigned int characters{0},frames{0};uint32_t counter{0};
  while(input >> characters >> counter >> frames) {
    tfx::presentation::text_reveal reveal{static_cast<uint16_t>(characters),counter};
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int frame{0}; frame != frames; ++frame) {
      uint32_t elapsed{0};bool skip{false};input >> elapsed >> skip;counter += elapsed;
      bool const consumed{reveal.advance(counter,skip)};
      auto const &state{reveal.state()};
      for(uint32_t const value : {state.previous,static_cast<uint32_t>(state.visible),static_cast<uint32_t>(state.remainder),static_cast<uint32_t>(consumed)}) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((value >> shift) & 255)) * 1099511628211ULL;
    }
    if(argc == 2) { uint64_t expected{0};if(!(input >> expected) || digest != expected) throw std::runtime_error{"text reveal clock differs from original"}; } else std::cout << digest << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
