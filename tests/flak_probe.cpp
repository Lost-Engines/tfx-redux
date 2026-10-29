#include "fixture_check.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "world/flak.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::world::flak_state state;
  tfx::world::flak_pool pool;
  unsigned int cases{0},night{0};
  char command;
  while(input >> command) {
    std::vector<int64_t> events,draws;
    bool flash{false};
    if(command == 'I') {
      int delay;unsigned int phase,scatter,model;
      input >> delay >> phase >> scatter >> model >> state.centre.x >> state.centre.y >> night;
      state.delay = static_cast<int16_t>(delay);
      state.random.phase = static_cast<uint16_t>(phase);
      state.random.scatter = static_cast<uint16_t>(scatter);
      state.night_model = static_cast<uint8_t>(model);
      pool.reset();
    } else if(command == 'F') {
      vec3<int32_t> player;
      unsigned int cloud,radio,count;int layer;
      input >> player.x >> player.y >> player.z >> cloud >> layer >> radio >> count;
      std::vector<tfx::missions::flak_region> regions(count);
      for(auto &area : regions) input >> area.first.x >> area.first.y >> area.last.x >> area.last.y;
      if(auto const effect{tfx::world::advance_flak(state,{
        .player{vec3<int32_t>{player}},
        .regions{regions},
        .night{night != 0},
        .cloud_layer{cloud != 0},
        .layer_y{static_cast<int16_t>(layer)},
        .radio_idle{radio != 0},
      })}) {
        if(effect->sample) events.insert(events.end(),{0,1,16});
        if(effect->radio) events.push_back(1);
        if(effect->flash) { events.push_back(2);flash = true; }
        auto const slot{pool.spawn(*effect)};
        events.insert(events.end(),{3,slot,effect->model});
      }
      if(tfx::world::flak_visible(state,player)) for(auto const slot : pool.active_slots()) {
        auto const &record{pool.record(slot)};
        draws.insert(draws.end(),{slot,record.model,record.position.x,record.position.y,record.position.z});
      }
    } else throw std::runtime_error{"unknown flak command"};
    std::vector<int64_t> values{state.delay,state.random.phase,state.random.scatter,state.night_model,state.centre.x,state.centre.y,flash,flash,static_cast<int64_t>(events.size())};
    values.insert(values.end(),events.begin(),events.end());
    values.push_back(static_cast<int64_t>(draws.size() / 5));
    values.insert(values.end(),draws.begin(),draws.end());
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(argc == 2) { if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"flak mismatch at case " + std::to_string(cases)}; }
    else std::cout << digest << '\n';
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
