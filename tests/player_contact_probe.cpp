#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "missions/progress.h"
#include "world/impact.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  int prior;
  int fatal;
  int special;
  int pilot;
  int disabled;
  int collision;
  int ticks;
  int hold;
  int legacy;
  tfx::missions::setup setup;
  while(input >> prior >> fatal >> special >> pilot >> disabled >> collision >> ticks >> hold >> legacy) {
    tfx::missions::progress progress{setup};
    progress.player_ejected = disabled != 0;
    progress.death_ticks = static_cast<int16_t>(ticks);
    auto const effects{tfx::world::resolve_player_contact({
      .previously_destroyed{prior != 0},
      .destroyed{fatal != 0},
      .contact_explosion{special != 0},
      .pilot_active{pilot != 0},
    })};
    if(effects.collision) collision = 1;
    if(effects.release_motion_hold) {
      hold = 0;
      legacy = 0;
    }
    std::vector<int> events;
    if(effects.action == tfx::world::player_contact_action::restore_destroyed) {
      events = {1, special ? 3 : 1, 17};
    } else if(effects.action == tfx::world::player_contact_action::explode) {
      progress.begin_contact_explosion();
      special = 1;
      events = {0, 27, 1, 3, 0, 2, 12};
    }
    std::vector<int> values{progress.player_ejected, collision, progress.death_ticks, hold, legacy, special};
    values.push_back(static_cast<int>(events.size()));
    values.insert(values.end(), events.begin(), events.end());
    for(auto const value : values) {
      if(argc == 2) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"contact mismatch at case " + std::to_string(cases)};
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
