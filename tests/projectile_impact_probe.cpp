#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include "weapons/definition.h"
#include "weapons/projectile.h"
#include "world/impact.h"

int main(int const argc, char const *const argv[]) try {
  if(argc >= 2 && std::string_view{argv[1]} == "--player-contacts") {
    std::ifstream fixture;
    if(argc == 3) fixture.open(argv[2]);
    if(argc > 3 || (argc == 3 && !fixture)) throw std::runtime_error{"expected --player-contacts [FILE]"};
    auto &input{argc == 3 ? fixture : std::cin};
    unsigned int cases{0};
    unsigned int weapon;
    unsigned int motion;
    unsigned int owned;
    unsigned int invulnerable;
    unsigned int ejected;
    unsigned int pilot;
    unsigned int source_destroyed;
    while(input >> weapon >> motion >> owned >> invulnerable >> ejected >> pilot >> source_destroyed) {
      bool const query{tfx::weapons::has_swept_contacts(static_cast<tfx::weapons::flight_stage>(motion)) && !owned && !invulnerable};
      if(argc == 3) {
        bool expected;
        if(!(input >> expected) || query != expected) throw std::runtime_error{"player contact gate mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << query << '\n';
      }
      ++cases;
    }
    return cases ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if(argc == 3 && std::string_view{argv[1]} == "--definitions") {
    auto const definitions{tfx::weapons::load_definitions(tfx::resources::read_file(argv[2]))};
    for(unsigned int index{0}; index != definitions.size(); ++index) std::cout << index << ' ' << static_cast<unsigned int>(definitions[index].category) << ' ' << definitions[index].impact_effects << '\n';
    return EXIT_SUCCESS;
  }
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int flags;
  unsigned int mode;
  unsigned int weapon;
  unsigned int player;
  int32_t y;
  int previous;
  int sampled;
  while(input >> flags >> mode >> weapon >> player >> y >> previous >> sampled) {
    int16_t appearance{static_cast<int16_t>(previous)};
    bool const spent{(flags & 1) != 0 && y >= 0};
    std::vector<int> events;
    if(spent) {
      auto const effects{tfx::world::select_projectile_impact(appearance, {
        .weapon{static_cast<uint8_t>(weapon)},
        .sample_surface{mode != 2},
        .player_owned{player != 0},
        .sampled_effect{sampled == -1 ? std::nullopt : std::optional{static_cast<tfx::world::effect_kind>(sampled)}},
      })};
      if(effects.visual) {
        if(*effects.visual == tfx::world::effect_kind::cannon_hit) events.insert(events.end(), {0, 1, 14});
        events.insert(events.end(), {1, std::to_underlying(*effects.visual)});
      }
      if(effects.explosion) events.insert(events.end(), {0, 1, 12});
    }
    std::vector<int> values{appearance, spent, static_cast<int>(events.size())};
    values.insert(values.end(), events.begin(), events.end());
    for(auto const value : values) {
      if(argc == 2) {
        int expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"projectile impact mismatch at case " + std::to_string(cases)};
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
