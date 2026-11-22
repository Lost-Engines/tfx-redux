#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "flight/defence.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int key;
  unsigned int aircraft;
  unsigned int chaff;
  unsigned int flares;
  unsigned int ecm;
  unsigned int stealth;
  unsigned int ejecting;
  unsigned int collision;
  unsigned int disabled;
  unsigned int afterburner;
  while(input >> key >> aircraft >> chaff >> flares >> ecm >> stealth >> ejecting >> collision >> disabled >> afterburner) {
    tfx::flight::defensive_systems state{
      .chaff{static_cast<uint8_t>(chaff)},
      .flares{static_cast<uint8_t>(flares)},
      .ecm{ecm != 0},
      .stealth{stealth != 0},
    };
    auto const result{tfx::flight::press_defensive_key(state, static_cast<tfx::flight::defensive_key>(key), {
      .aircraft{static_cast<tfx::flight::player_aircraft>(aircraft)},
      .ejecting{ejecting != 0},
      .collision{collision != 0},
      .disabled{disabled != 0},
      .afterburner{static_cast<uint8_t>(afterburner)},
    })};
    std::vector<int> values{state.chaff, state.flares, state.ecm, state.stealth, result.clear_targeting, result.release.has_value()};
    if(result.release) values.insert(values.end(), {result.release->radar, result.release->strength});
    for(auto const value : values) {
      if(argc == 2) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"defensive control mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << value << ' ';
      }
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
