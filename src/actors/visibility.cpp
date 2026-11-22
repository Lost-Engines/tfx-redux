#include "visibility.h"

namespace tfx::actors {
namespace {

bool inside(vec3<int32_t> const &position, vec3<int32_t> const &reference, bool const vertical) {
  int32_t constexpr maximum{actor_draw_range * position_unit};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    if(axis == 1 && !vertical) continue;
    int32_t const delta{std::bit_cast<int32_t>(static_cast<uint32_t>(position[axis]) - static_cast<uint32_t>(reference[axis]))};
    // Native C96BC keeps INT_MIN negative; the following signed test accepts it.
    int32_t const magnitude{delta < 0 ? std::bit_cast<int32_t>(0u - static_cast<uint32_t>(delta)) : delta};
    if(magnitude > maximum) return false;
  }
  return true;
}

} // anonymous namespace

bool aircraft_in_draw_region(vec3<int32_t> const &position, vec3<int32_t> const &reference) {
  return inside(position,reference,false);                                     // Native 0x78180: X and Z, inclusive.
}
bool surface_in_draw_region(vec3<int32_t> const &position, vec3<int32_t> const &reference) {
  return inside(position,reference,true);                                      // Native 0x7c354: X, Y and Z, inclusive.
}

} // namespace tfx::actors
