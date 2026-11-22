#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include "audio/requests.h"
#include "missions/progress.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0},count,weapon,player,arcade,training,present,before,after;
  int x,z,violations;
  while(input >> x >> z >> weapon >> player >> arcade >> training >> present >> before >> after >> violations >> count) {
    tfx::missions::setup setup;
    for(unsigned int index{0}; index != count; ++index) {
      tfx::missions::target target;
      if(!(input >> target.position.x >> target.position.y >> target.position.z)) throw std::runtime_error{"incomplete targets"};
      setup.targets.push_back(std::move(target));
    }
    tfx::missions::progress state{setup};
    state.violations = static_cast<int16_t>(violations);
    tfx::missions::terrain_loss_effects effects;
    if(present && !before && after) effects = state.record_terrain_loss({
      .cell{x,z},
      .player_owned{player != 0},
      .weapon{static_cast<uint8_t>(weapon)},
      .arcade{arcade != 0},
      .training{training != 0},
    });
    std::vector<int> events;
    if(effects.unauthorised) events.insert(events.end(),{0,std::to_underlying(tfx::audio::music_cue_id::unauthorised_target)});
    if(effects.explosion) {
      events.insert(events.end(),{1,1,effects.unauthorised ? 2 : 1,std::to_underlying(tfx::audio::sample_id::aircraft_explosion)});
      if(effects.unauthorised) events.push_back(std::to_underlying(tfx::audio::sample_id::unauthorised_target));
    }
    if(effects.notice == tfx::missions::terrain_loss_notice::training_warning) events.insert(events.end(),{2,0});
    else if(effects.notice == tfx::missions::terrain_loss_notice::violation) events.insert(events.end(),{3,1});
    std::vector<int> values{state.violations,static_cast<int>(events.size())};
    values.insert(values.end(),events.begin(),events.end());
    for(auto const value : values) {
      if(argc == 2) { int expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"terrain credit mismatch at case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
