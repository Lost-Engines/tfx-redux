#include "fixture_check.h"
#include <bit>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include "flight/ejection.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"ejection probe accepts an optional native fixture"};
  std::ifstream fixture;
  if(argc == 2) { fixture.open(argv[1]);if(!fixture) throw std::runtime_error{"cannot open native pilot fixture"}; }
  std::istream &input{argc == 2 ? fixture : std::cin};
  unsigned int count{0};
  tfx::flight::pilot_motion pilot;
  unsigned int reset,phase,parachute,ended,fatal,ticks,water;int32_t speed,counter,auxiliary;int collision;
  vec3f position,velocity;tfx::math::attitude basis;
  while(input >> reset >> phase) {
    for(auto *vector : {&position,&velocity}) for(auto &value : *vector) { uint32_t bits;input >> bits;value = std::bit_cast<float>(bits); }
    for(unsigned int row{0}; row != 3; ++row) input >> basis.right[row] >> basis.down[row] >> basis.forward[row];
    input >> speed >> counter >> parachute >> auxiliary >> collision >> ended >> fatal;
    tfx::flight::pilot_context context;
    input >> ticks >> context.ground_height >> context.terrain_type >> water;
    context.elapsed = static_cast<tfx::simulation::timer_ticks>(ticks);context.water_world = water != 0;
    if(!input) throw std::runtime_error{"incomplete ejection input"};
    if(reset) pilot = {
      .phase{static_cast<tfx::flight::pilot_phase>(phase)},
      .position{vec3f{position}},
      .velocity{vec3f{velocity}},
      .orientation{basis},
      .aircraft_speed{speed},
      .elapsed{counter},
      .parachute_open{parachute != 0},
      .auxiliary_counter{auxiliary},
      .collision{static_cast<int16_t>(collision)},
      .ended{ended != 0},
      .fatal{fatal != 0},
    };
    auto const update{tfx::flight::advance_pilot(pilot,context)};
    std::vector<int64_t> values{static_cast<unsigned int>(pilot.phase)};
    for(auto const *vector : {&pilot.position,&pilot.velocity}) for(auto const value : *vector) values.push_back(std::bit_cast<uint32_t>(value));
    for(unsigned int row{0}; row != 3; ++row) { values.push_back(pilot.orientation.right[row]);values.push_back(pilot.orientation.down[row]);values.push_back(pilot.orientation.forward[row]); }
    for(auto const value : {pilot.aircraft_speed,pilot.elapsed,static_cast<int32_t>(pilot.parachute_open),pilot.auxiliary_counter,static_cast<int32_t>(pilot.collision),static_cast<int32_t>(pilot.ended),static_cast<int32_t>(pilot.fatal)}) values.push_back(value);
    for(auto const value : pilot.integer_position) values.push_back(value);
    for(auto const value : pilot.angles) values.push_back(value);
    values.push_back(static_cast<int64_t>(update.model_entries.size()));for(auto const entry : update.model_entries) values.push_back(entry);
    if(argc == 2) {
      uint64_t digest{14695981039346656037ull};
      for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ static_cast<uint8_t>(static_cast<uint32_t>(value) >> shift)) * 1099511628211ull;
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native pilot fixture differs at case " + std::to_string(count)};
    } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
    ++count;
  }
  if(argc == 2) { if(!count) throw std::runtime_error{"empty native pilot fixture"};std::cout << count << " native ejected-pilot fixtures passed\n"; }
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
