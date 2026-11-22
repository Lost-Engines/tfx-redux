#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include "flight/damage.h"

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2};
  std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read damage fixture"};
  int32_t severity{0};
  unsigned int invulnerable{0};
  unsigned int elapsed{0};
  unsigned int low{0};
  unsigned int high{0};
  int display_selection{0};
  uint64_t accumulated{0};
  while(input >> severity >> invulnerable >> elapsed >> low >> high >> accumulated >> display_selection) {
    tfx::flight::damage_state state;
    state.accumulated = std::bit_cast<double>(accumulated);
    state.right_display_selection = static_cast<int16_t>(display_selection);
    for(auto &condition : state.systems) {
      unsigned int value{0};
      input >> value;
      condition = static_cast<tfx::flight::system_condition>(value);
    }
    for(auto &ticks : state.repair_ticks) {
      unsigned int value{0};
      input >> value;
      ticks = static_cast<uint16_t>(value);
    }
    tfx::math::random_state random{static_cast<uint16_t>(low), static_cast<uint16_t>(high)};
    unsigned int notifications{0};
    tfx::flight::apply_damage(state, severity, random, invulnerable != 0, [&]{++notifications;});
    tfx::flight::repair_systems(state, static_cast<uint16_t>(elapsed));
    std::vector<uint64_t> values{random.low, random.high, std::bit_cast<uint64_t>(state.accumulated), static_cast<uint16_t>(state.right_display_selection), notifications};
    for(auto const condition : state.systems) values.push_back(std::to_underlying(condition));
    for(auto const ticks : state.repair_ticks) values.push_back(ticks);
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) {
      for(unsigned int shift{0}; shift != 64; shift += 8) digest = (digest ^ ((value >> shift) & 255)) * 1099511628211ULL;
    }
    if(check) {
      uint64_t expected{0};
      if(!(input >> expected) || expected != digest) throw std::runtime_error{"native damage/repair state differs"};
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
