#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include "world/effects.h"
#include "world/instance.h"

int main(int const argc, char const *const argv[]) try {
  if(argc == 2 && std::string_view{argv[1]} == "--request-order") {
    std::vector<uint8_t> raw(16);
    raw[0] = 2;
    raw[8] = 3;
    auto const entry{[&](unsigned int const index){
      auto const field{10 + index * 2};
      auto const offset{static_cast<uint16_t>((raw.size() - field) / 2)};
      raw[field] = static_cast<uint8_t>(offset);
      raw[field + 1] = static_cast<uint8_t>(offset >> 8);
    }};
    auto const emit{[&](std::initializer_list<uint16_t> words){
      for(auto const word : words) {
        raw.push_back(static_cast<uint8_t>(word));
        raw.push_back(static_cast<uint8_t>(word >> 8));
      }
    }};
    entry(0);
    emit({11, 0, 0, 0});
    entry(1);
    emit({11, 0, 11, 79, 1, 2, 3, 3, 11, 0, 22, 79, 65531, 65530, 65529, 9, 11, 0, 33, 0});
    entry(2);
    emit({0});
    tfx::world::definition const definition{raw, std::span<std::string const>{}};
    tfx::world::instance state{definition};
    std::vector<unsigned int> observations;
    auto const output{[&](tfx::world::scene_request const &request){
      observations.push_back(state.custom.at(0));
      observations.push_back(request.opcode);
      observations.insert(observations.end(), request.arguments.begin(), request.arguments.end());
    }};
    state.execute(1, {
      .request_output{output},
    });
    std::vector<unsigned int> const expected{11, 79, 1, 2, 3, 3, 22, 79, 65531, 65530, 65529, 9};
    if(observations != expected || state.custom.at(0) != 33) throw std::runtime_error{"SSD request delivery was deferred or reordered"};
    for(auto const value : observations) std::cout << value << ' ';
    std::cout << '\n';
    return EXIT_SUCCESS;
  }
  if(argc >= 2 && std::string_view{argv[1]} == "--place") {
    std::ifstream fixture;
    if(argc == 3) fixture.open(argv[2]);
    if(argc > 3 || (argc == 3 && !fixture)) throw std::runtime_error{"expected --place [FILE]"};
    auto &input{argc == 3 ? fixture : std::cin};
    unsigned int cases{0};
    unsigned int kind;
    unsigned int heading;
    vec3<int16_t> local;
    vec3<int16_t> origin;
    vec3<int32_t> reference;
    while(input >> kind >> local.x >> local.y >> local.z >> origin.x >> origin.y >> origin.z >> heading >> reference.x >> reference.y >> reference.z) {
      auto const point{tfx::world::place_instance_point(local, origin, static_cast<tfx::math::angle>(heading))};
      std::array<int, 5> const values{point.x, point.y, point.z, static_cast<int>(kind), tfx::world::instance_point_distance(local, origin, static_cast<tfx::math::angle>(heading), vec3<int16_t>{reference})};
      for(auto const value : values) {
        if(argc == 3) {
          int expected;
          if(!(input >> expected) || value != expected) throw std::runtime_error{"effect placement mismatch at case " + std::to_string(cases)};
        } else {
          std::cout << value << ' ';
        }
      }
      if(argc != 3) std::cout << '\n';
      ++cases;
    }
    return cases ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::world::effect_pool effects;
  char command;
  unsigned int cases{0};
  while(input >> command) {
    std::vector<int64_t> events;
    if(command == 'R') {
      effects.reset();
    } else if(command == 'S') {
      vec3<int32_t> position;
      unsigned int kind;
      if(!(input >> position.x >> position.y >> position.z >> kind)) throw std::runtime_error{"incomplete effect spawn"};
      if(kind == std::to_underlying(tfx::world::effect_kind::cannon_hit)) events = {0, 1, 14};
      auto const slot{effects.spawn(position, static_cast<tfx::world::effect_kind>(kind))};
      events.insert(events.end(), {1, slot, kind});
    } else {
      throw std::runtime_error{"unknown effect probe command"};
    }
    std::vector<int64_t> values;
    for(auto const slot : effects.active_slots()) {
      auto const &effect{effects.record(slot)};
      values.insert(values.end(), {slot, std::to_underlying(effect.kind), effect.position.x, effect.position.y, effect.position.z});
    }
    values.insert(values.begin(), static_cast<int64_t>(values.size() / 5));
    values.push_back(static_cast<int64_t>(events.size()));
    values.insert(values.end(), events.begin(), events.end());
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) {
      for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    }
    if(argc == 2) {
      uint64_t expected;
      if(!(input >> expected) || expected != digest) throw std::runtime_error{"effect pool mismatch at case " + std::to_string(cases)};
    } else {
      std::cout << digest << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
