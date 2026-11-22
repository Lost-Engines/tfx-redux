#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "weapons/projectile.h"
#include "world/trails.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::world::trail_pool pool;
  std::array<bool,tfx::world::trail_capacity> dead{};
  unsigned int cases{0};char command;
  while(input >> command) {
    std::vector<int64_t> values,events;
    if(command == 'C') {
      unsigned int flags,enabled,motion;int32_t distance;vec3<int32_t> previous,current;
      input >> flags >> enabled >> motion >> distance >> previous.x >> previous.y >> previous.z >> current.x >> current.y >> current.z;
      bool const request{(flags & 1) != 0 && enabled && tfx::weapons::has_smoke_trail(static_cast<tfx::weapons::flight_stage>(motion),distance)};
      values.push_back(request);
      if(request) for(auto const *const p : {&previous,&current}) for(auto const v : {p->x,p->y,p->z}) values.push_back(tfx::math::signed_word(static_cast<uint16_t>(v >> tfx::actors::position_fraction_bits)));
    } else if(command == 'S') {
      unsigned int enabled,visible;vec3<int32_t> previous,current;
      input >> enabled >> visible >> previous.x >> previous.y >> previous.z >> current.x >> current.y >> current.z;
      auto const segment{tfx::world::prepare_trail(previous,current,enabled != 0)};
      values.push_back(segment.has_value());
      if(segment) {
        auto const middle{segment->midpoint()};
        values.insert(values.end(),{segment->length,middle.x,middle.y,middle.z});
        if(visible) {
          auto const output{[&](vec3<int32_t> const &p){ events.insert(events.end(),{p.x,p.y,p.z}); }};
          tfx::world::emit_trail(*segment,output);
        }
      }
      values.push_back(static_cast<int64_t>(events.size() / 3));
      values.insert(values.end(),events.begin(),events.end());
    } else {
      if(command == 'R') { pool.reset();dead.fill(false); } else if(command == 'P') {
        unsigned int enabled;vec3<int32_t> p;
        input >> enabled >> p.x >> p.y >> p.z;
        if(enabled) {
          auto const slot{pool.spawn(p)};dead[slot] = false;
          events.insert(events.end(),{0,slot,p.x,p.y,p.z});
        }
      } else if(command == 'T') {
        if(!pool.active_slots().empty()) pool.retire_oldest();
      } else if(command == 'D') {
        vec3<int32_t> reference;unsigned int count;
        input >> reference.x >> reference.y >> reference.z >> count;
        for(unsigned int i{0}; i != count; ++i) { unsigned int slot;input >> slot;dead.at(slot) = true; }
        for(auto const slot : pool.active_slots()) {
          auto const world{tfx::world::trail_position(pool.record(slot).position_q8,reference)};
          auto const encoded{[&](double const value){ return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(value * 65536))); }};
          events.insert(events.end(),{1,slot,encoded(world.x),encoded(world.y),encoded(world.z)});
        }
        auto const destroyed{[&](tfx::world::trail_slot const slot){ return dead.at(slot); }};
        pool.clean(reference,destroyed);
      } else throw std::runtime_error{"unknown trail probe command"};
      for(auto const slot : pool.active_slots()) {
        auto const &p{pool.record(slot).position_q8};
        values.insert(values.end(),{slot,p.x,p.y,p.z});
      }
      values.insert(values.begin(),static_cast<int64_t>(values.size() / 4));
      values.push_back(static_cast<int64_t>(events.size()));
      values.insert(values.end(),events.begin(),events.end());
    }
    if(!input) throw std::runtime_error{"incomplete trail probe input"};
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(argc == 2) { uint64_t expected;if(!(input >> expected) || expected != digest) throw std::runtime_error{"native trail mismatch at case " + std::to_string(cases)}; } else std::cout << digest << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
