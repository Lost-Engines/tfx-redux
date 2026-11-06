#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "flight/arcade.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected optional engine-indicator fixture"};
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot open engine-indicator fixture"};
  unsigned int aircraft{0},factors{0},left{0},right{0};
  unsigned int count{0};
  while(input >> aircraft >> factors >> left >> right) {
    tfx::flight::arcade_state state;
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft);
    tfx::flight::initialise_arcade(state,{293888,-1800,236544},{0,0,0},.65f,false);
    state.left_engine = factors & 1;
    state.right_engine = factors & 2;
    tfx::flight::update_basic_speed(state,{},
      {static_cast<tfx::flight::system_condition>(left),static_cast<tfx::flight::system_condition>(right)},90,true,false);
    std::array<uint64_t,4> const result{
      std::bit_cast<uint32_t>(state.target_speed),
      std::bit_cast<uint32_t>(state.speed_mach),
      std::bit_cast<uint64_t>(state.unrounded_speed_mach),
      std::bit_cast<uint32_t>(state.throttle),
    };
    if(argc == 2) {
      for(auto const value : result) {
        uint64_t expected{0};
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native engine-indicator case differs: " + std::to_string(count)};
      }
    } else {
      for(auto const value : result) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++count;
  }
  if(!count) throw std::runtime_error{"empty engine-indicator input"};
  return 0;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return 1;
}
