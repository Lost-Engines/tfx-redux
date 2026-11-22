#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/atmosphere.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open atmosphere fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int realistic,cases{0};
  int16_t temperature;
  while(input >> realistic >> temperature) {
    auto const height{read_double(input)}, down{read_double(input)};
    tfx::flight::atmosphere_parameters const parameters{
      .sea_level_density{read_double(input)},
      .temperature_lapse_per_metre{read_double(input)},
      .temperature_lapse_limit_metres{read_double(input)},
    };
    auto const sample{tfx::flight::sample_atmosphere(height,down,realistic ? std::optional<int16_t>{temperature} : std::nullopt,parameters)};
    for(auto const value : {sample.density,sample.sea_level_kelvin,sample.local_kelvin,sample.sound_metres_per_second}) {
      auto const actual{std::bit_cast<uint64_t>(value)};
      if(argc == 2) {
        uint64_t expected;
        if(!(input >> expected) || actual != expected) throw std::runtime_error{"native atmosphere differs"};
      } else std::cout << actual << ' ';
    }
    if(argc == 1) std::cout << '\n';
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no atmosphere cases"};
  if(argc == 2) std::cout << cases << " native atmosphere fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
