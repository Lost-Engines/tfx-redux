#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "weapons/runtime.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  bool const population{argc == 2 && std::string_view{argv[1]} == "--population"};
  if(argc == 2 && !population) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !population && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 && !population ? fixture : std::cin};
  unsigned int cases{0}, elapsed, enabled, inhibited, low, high, count;
  int cooldown;
  unsigned int frames{1};
  while((!population || static_cast<bool>(input >> frames)) && (input >> cooldown >> low >> high >> elapsed >> enabled >> inhibited >> count)) {
    tfx::weapons::warning_state state{
      .cooldown{static_cast<int16_t>(cooldown)},
    };
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    tfx::weapons::begin_warning_step(state,static_cast<uint16_t>(elapsed));
    std::vector<int> events;
    auto const output{[&](tfx::weapons::projectile_warning const &warning){
      if(warning.music) events.insert(events.end(),{0,9});
      events.insert(events.end(),{1,2,static_cast<int>(warning.sample)});
    }};
    struct threat_input {
      int distance;
      bool proximity;
      bool targeted;
    };
    std::vector<threat_input> threats;
    for(unsigned int index{0}; index != count; ++index) {
      int distance;
      unsigned int proximity, targeted;
      input >> distance >> proximity >> targeted;
      threats.push_back({
        .distance{distance},
        .proximity{proximity != 0},
        .targeted{targeted != 0},
      });
      if(!population && enabled && proximity) if(auto const warning{tfx::weapons::observe_player_threat(state,distance,targeted != 0,inhibited != 0,random)}) {
        output(*warning);
      }
    }
    if(!population) if(auto const warning{tfx::weapons::finish_warning_step(state)}) output(*warning);
    if(population) {
      if(cooldown) throw std::runtime_error{"population cases begin with the native zero cooldown"};
      std::vector<tfx::weapons::definition> definitions(count);
      for(unsigned int index{0}; index != count; ++index) {
        definitions[index].player_proximity = threats[index].proximity;
        definitions[index].stages.push_back({
          .ticks{1},
          .motion{tfx::weapons::flight_stage::finished},
        });
      }
      tfx::weapons::runtime projectiles{definitions,{
        .actor_position{[](tfx::weapons::actor_identity){ return vec3<int32_t>{0,-25600,0}; }},
        .motion_inputs{[&](tfx::weapons::active_projectile const&,uint16_t const ticks){
          return tfx::weapons::motion_context{
            .elapsed{ticks},
            .target{0,0,0},
            .player_ejected{inhibited != 0},
          };
        }},
        .player_distance{[&](tfx::weapons::active_projectile const &shot){ return threats[shot.state.weapon].distance; }},
        .warning{output},
      }};
      for(unsigned int index{count}; index--;) projectiles.launch({
        .weapon{index},
        .position{0,-25600,0},
        .angles{0,0,0},
        .speed{0},
        .owner{
          .pool{tfx::weapons::actor_pool::aircraft},
        },
        .target{
          .actor{tfx::weapons::actor_identity{.pool{threats[index].targeted ? tfx::weapons::actor_pool::player : tfx::weapons::actor_pool::aircraft}}},
        },
      });
      for(unsigned int frame{0}; frame != frames; ++frame) projectiles.advance(static_cast<uint16_t>(elapsed),random,enabled != 0);
      state = projectiles.player_warnings();
    }
    std::vector<int> values{state.cooldown,state.incoming,random.low,random.high,static_cast<int>(events.size())};
    values.insert(values.end(),events.begin(),events.end());
    for(auto const value : values) {
      if(argc == 2 && !population) { int expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"projectile warning mismatch at case " + std::to_string(cases)}; } else std::cout << value << ' ';
    }
    if(argc != 2 || population) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
