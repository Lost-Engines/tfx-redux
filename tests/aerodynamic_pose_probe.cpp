#include "fixture_check.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_pose.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits;input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic pose fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int aircraft,cases{0};
  while(input >> aircraft) {
    tfx::flight::aerodynamic_pose state;
    for(auto &value : state.metres_ned) value = read_double(input);
    for(unsigned int row{0}; row != 3; ++row) input >> state.frame.forward[row] >> state.frame.right[row] >> state.frame.down[row];
    tfx::flight::aerodynamic_viewpoint const offset{
      .forward{read_double(input)},
      .upward{read_double(input)},
    };
    auto const view{tfx::flight::update_aerodynamic_view(state,static_cast<tfx::flight::player_aircraft>(aircraft),offset)};
    std::vector<uint32_t> values;
    for(unsigned int row{0}; row != 3; ++row) {
      values.push_back(static_cast<uint32_t>(state.frame.forward[row]));
      values.push_back(static_cast<uint32_t>(state.frame.right[row]));
      values.push_back(static_cast<uint32_t>(state.frame.down[row]));
    }
    for(auto const *basis : {&view.orientation,&view.orientation}) for(unsigned int row{0}; row != 3; ++row) {
      values.push_back(static_cast<uint32_t>(basis->right[row]));
      values.push_back(static_cast<uint32_t>(basis->down[row]));
      values.push_back(static_cast<uint32_t>(basis->forward[row]));
    }
    for(auto const value : view.position) values.push_back(std::bit_cast<uint32_t>(value));
    for(auto const value : view.integer_position) values.push_back(static_cast<uint32_t>(value));
    for(auto const value : view.angles) values.push_back(value);
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic world pose differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic pose cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic pose fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
