#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "world/terrain_cells.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"terrain cells require an optional fixture file"};
  auto &input{argc == 2 ? static_cast<std::istream &>(fixture) : std::cin};
  int32_t x;
  int32_t z;
  int32_t height;
  int width;
  unsigned int cases{0};
  while(input >> x >> z >> width >> height) {
    uint64_t hash{14695981039346656037ull};
    unsigned int count{0};
    tfx::world::visit_terrain_cells({x, z}, static_cast<int16_t>(width), [&](auto const &cell){
      ++count;
      uint32_t const source{cell.source.x * tfx::world::map_height + tfx::world::map_height - 1 - cell.source.y};
      int32_t const px{tfx::math::signed_word(static_cast<uint16_t>((static_cast<uint32_t>(cell.world_cell.x) * 2u + 1u) << 10))};
      int32_t const pz{tfx::math::signed_word(static_cast<uint16_t>((static_cast<uint32_t>(cell.world_cell.y) * 2u + 1u) << 10))};
      for(uint32_t const value : {source, static_cast<uint32_t>(px), static_cast<uint32_t>(tfx::math::signed_word(static_cast<uint16_t>(height))), static_cast<uint32_t>(pz)}) {
        for(unsigned int shift{0}; shift != 32; shift += 8) hash = (hash ^ static_cast<uint8_t>(value >> shift)) * 1099511628211ull;
      }
    });
    if(argc == 2) {
      unsigned int expected_count;
      uint64_t expected_hash;
      if(!(input >> expected_count >> expected_hash) || count != expected_count || hash != expected_hash) throw std::runtime_error{"native terrain traversal differs at case " + std::to_string(cases)};
    } else {
      std::cout << count << ' ' << hash << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no terrain cell cases"};
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
