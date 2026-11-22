#include "contact_region.h"
#include <algorithm>

namespace tfx::actors {
namespace {

int32_t subtract(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? subtract(0,value) : value;
}

} // anonymous namespace

contact_region nearby_surface_region(vec3<int32_t> const &previous, vec3<int32_t> const &current) {
  // Native 0x7c528 encloses the player's swept world-integer segment, then expands every radius to at least 1,024.
  int32_t constexpr minimum_radius{1024};
  unsigned int constexpr position_fraction_bits{8};
  contact_region result;
  for(unsigned int axis{0}; axis != 3; ++axis) {
    int32_t const sum{std::bit_cast<int32_t>(static_cast<uint32_t>(previous[axis]) + static_cast<uint32_t>(current[axis]))};
    int32_t const centre{sum / 2};
    int32_t const radius{std::max({minimum_radius,magnitude(subtract(previous[axis],centre)),magnitude(subtract(current[axis],centre))})};
    result.centre[axis] = std::bit_cast<int32_t>(static_cast<uint32_t>(centre) << position_fraction_bits);
    result.extent[axis] = std::bit_cast<int32_t>(static_cast<uint32_t>(radius) << position_fraction_bits);
  }
  return result;
}

bool contact_region::contains(vec3<int32_t> const &position) const {
  // 0x7c724 tests all three Q8 coordinates strictly inside the region; its edge is excluded.
  for(unsigned int axis{0}; axis != 3; ++axis) if(magnitude(subtract(position[axis],centre[axis])) >= extent[axis]) return false;
  return true;
}

} // namespace tfx::actors
