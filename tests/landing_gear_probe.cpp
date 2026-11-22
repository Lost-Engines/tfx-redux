#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "flight/landing_gear.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open landing gear fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  bool down;
  unsigned int cases{0};
  while(input >> down) {
    unsigned int condition;
    uint32_t vertical;
    uint32_t speed;
    tfx::flight::landing_gear_context context;
    input >> condition >> vertical >> context.forward_y >> speed >> context.landed >> context.previous_landed >> context.airborne_speed_limit >> context.ground_speed_limit >> context.touchdown_descent_limit;
    context.metres_per_second = std::bit_cast<float>(speed);
    tfx::flight::landing_gear_state state{
      .down{down},
      .condition{static_cast<tfx::flight::system_condition>(condition)},
      .vertical_speed{std::bit_cast<float>(vertical)},
    };
    auto const sounds{tfx::flight::update_landing_gear(state, context)};
    std::vector<uint32_t> result{state.down, std::to_underlying(state.condition), std::bit_cast<uint32_t>(state.vertical_speed), static_cast<uint32_t>(sounds.size())};
    result.insert(result.end(), sounds.begin(), sounds.end());
    if(argc == 2) {
      for(auto const value : result) {
        uint32_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native landing gear fixture differs"};
      }
    } else {
      for(auto const value : result) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no landing gear cases"};
  if(argc == 2) std::cout << cases << " native landing gear fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
