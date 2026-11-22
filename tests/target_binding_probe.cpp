#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "missions/target_binding.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected an optional target-binding fixture"};
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open target-binding fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  vec3<int32_t> position;
  unsigned int fixed_count;
  unsigned int surface_count;
  unsigned int count{0};
  while(input >> position.x >> position.y >> position.z >> fixed_count >> surface_count) {
    if(fixed_count > 100 || surface_count > tfx::actors::surface_capacity) throw std::runtime_error{"target-binding fixture exceeds its actor limit"};
    std::vector<vec3<int32_t>> fixed;
    for(unsigned int index{0}; index != fixed_count; ++index) {
      vec3<int32_t> point;
      input >> point.x >> point.y >> point.z;
      fixed.emplace_back(point);
    }
    std::vector<tfx::missions::surface_target_candidate> surface;
    for(unsigned int index{0}; index != surface_count; ++index) {
      tfx::missions::surface_target_candidate candidate;
      unsigned int flags;
      input >> candidate.slot >> candidate.position_q8.x >> candidate.position_q8.y >> candidate.position_q8.z >> flags;
      candidate.flags = static_cast<uint8_t>(flags);
      surface.push_back(candidate);
    }
    if(!input) throw std::runtime_error{"truncated target-binding fixture"};
    auto const result{tfx::missions::bind_target(position, fixed, surface)};
    std::array<int32_t, 4> const values{std::visit([](auto const &reference)->std::array<int32_t, 4> {
      using reference_type = std::decay_t<decltype(reference)>;
      if constexpr(std::is_same_v<reference_type, tfx::missions::fixed_target>) {
        return {0, static_cast<int32_t>(reference.index), 0, 0};
      } else if constexpr(std::is_same_v<reference_type, tfx::missions::surface_target>) {
        return {1, static_cast<int32_t>(reference.slot), 0, 0};
      } else {
        return {2, 0, reference.cell.x, reference.cell.y};
      }
    }, result)};
    if(argc == 2) {
      std::array<int32_t, 4> expected;
      for(auto &value : expected) input >> value;
      if(!input || values != expected) throw std::runtime_error{"native target binding differs in case " + std::to_string(count)};
    } else {
      std::cout << values[0] << ' ' << values[1] << ' ' << values[2] << ' ' << values[3] << '\n';
    }
    ++count;
  }
  if(!count) throw std::runtime_error{"empty target-binding input"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
