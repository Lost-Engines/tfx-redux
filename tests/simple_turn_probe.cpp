#include "fixture_check.h"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/simple_turn.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open Simple turn fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  uint32_t mach,pitch;
  while(input >> mach >> pitch) {
    unsigned int elapsed;input >> elapsed;
    tfx::flight::simple_turn_context const context{
      .mach{std::bit_cast<float>(mach)},
      .pitch_rate{std::bit_cast<float>(pitch)},
      .elapsed{static_cast<tfx::simulation::timer_ticks>(elapsed)},
    };
    tfx::flight::simple_turn_state state;
    for(unsigned int row{0}; row != 3; ++row) input >> state.orientation.right[row] >> state.orientation.down[row] >> state.orientation.forward[row];
    uint32_t remainder;input >> remainder >> state.bank_correction;
    state.heading_remainder = std::bit_cast<float>(remainder);
    tfx::flight::update_simple_turn(state,context);
    std::array<uint32_t,17> values;
    for(unsigned int row{0}; row != 3; ++row) {
      values[row * 3] = static_cast<uint32_t>(state.orientation.right[row]);
      values[row * 3 + 1] = static_cast<uint32_t>(state.orientation.down[row]);
      values[row * 3 + 2] = static_cast<uint32_t>(state.orientation.forward[row]);
    }
    values[9] = std::bit_cast<uint32_t>(state.heading_remainder);
    values[10] = static_cast<uint32_t>(state.bank_correction);
    values[11] = std::bit_cast<uint32_t>(state.turn_response);
    values[12] = std::bit_cast<uint32_t>(state.movement_per_tick);
    values[13] = std::bit_cast<uint32_t>(state.coordinated_turn_rate);
    values[14] = std::bit_cast<uint32_t>(state.metres_per_second);
    values[15] = values[16] = std::bit_cast<uint32_t>(state.g_load);
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native Simple turn fixture differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no Simple turn cases"};
  if(argc == 2) std::cout << cases << " native Simple turn fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
