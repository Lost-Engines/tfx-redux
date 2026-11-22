#pragma once

#include <functional>
#include "world/environment.h"

namespace tfx::world {

int16_t constexpr terrain_neighbourhood_width{16};                             // Native DATA 0xaac4 in ordinary TFX environments.
struct terrain_cell {
  vec2<unsigned int> source;
  vec2<int32_t> world_cell;
};

template<typename Visitor>
void visit_terrain_cells(vec2<int32_t> const &position, int16_t const width, Visitor &&visit) {
  // Native 0xb0ba8: Z-major, half-open square around arithmetic-shifted cells.
  // Negative remainders add dimension - 1, retaining the original seam quirk.
  int const half{width >> 1};
  vec2<int32_t> const centre{position.x >> cell_shift,position.y >> cell_shift};
  auto const wrap{[](int32_t const value, unsigned int const dimension){
    int32_t const remainder{value % static_cast<int32_t>(dimension)};
    return static_cast<unsigned int>(remainder < 0 ? remainder + static_cast<int32_t>(dimension) - 1 : remainder);
  }};
  for(int z{-half}; z < half; ++z) {
    int32_t const world_z{centre.y + z};
    auto const row{map_height - 1 - wrap(static_cast<int32_t>(map_height) - 1 - world_z,map_height)};
    unsigned int column{wrap(centre.x - half,map_width)};
    for(int x{-half}; x < half; ++x) {
      int32_t const world_x{centre.x + x};
      std::invoke(visit,terrain_cell{
        .source{column,row},
        .world_cell{world_x,world_z},
      });
      // Only the first X remainder uses the seam adjustment; later columns
      // advance through the mapped row and wrap normally at its far edge.
      if(++column == map_width) column = 0;
    }
  }
}

} // namespace tfx::world
