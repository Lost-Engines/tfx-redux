#include "collision.h"
#include <bit>

namespace tfx::world {
namespace {

bool crosses_rectangle(vec2<int> const &minimum, vec2<int> const &maximum, vec2<int> const &a, vec2<int> const &b) {
  /// Native 0xd5dbc uses strict endpoint containment and a directed corner test, including its asymmetric tangent cases.
  auto const region{[&](vec2<int> const &p){
    return (p.x <= minimum.x ? 1u : 0u) | (p.x >= maximum.x ? 2u : 0u) | (p.y >= maximum.y ? 4u : 0u) | (p.y <= minimum.y ? 8u : 0u);
  }};
  auto const first{region(a)};
  auto const second{region(b)};
  if(!first || !second) return true;
  if(first & second) return false;
  auto const negative{[&](int const x, int const y){
    return static_cast<int64_t>(y - a.y) * (b.x - a.x) - static_cast<int64_t>(x - a.x) * (b.y - a.y) < 0;
  }};
  // Only adjacent outside regions need a corner test. Opposite corners test both diagonally opposed corners.
  switch(first | (second << 4)) {
  case 0x81:
  case 0x85:
  case 0xa1:
    return negative(minimum.x, minimum.y);
  case 0x28:
  case 0x29:
  case 0x68:
    return negative(maximum.x, minimum.y);
  case 0x42:
  case 0x4a:
  case 0x52:
    return negative(maximum.x, maximum.y);
  case 0x14:
  case 0x16:
  case 0x94:
    return negative(minimum.x, maximum.y);
  case 0x18:
  case 0x1a:
  case 0x58:
    return !negative(minimum.x, minimum.y);
  case 0x82:
  case 0x86:
  case 0x92:
    return !negative(maximum.x, minimum.y);
  case 0x24:
  case 0x25:
  case 0xa4:
    return !negative(maximum.x, maximum.y);
  case 0x41:
  case 0x49:
  case 0x61:
    return !negative(minimum.x, maximum.y);
  case 0x5a:
  case 0xa5:
    return negative(minimum.x, minimum.y) != negative(maximum.x, maximum.y);
  case 0x69:
  case 0x96:
    return negative(maximum.x, minimum.y) != negative(minimum.x, maximum.y);
  case 0x12:
  case 0x21:
  case 0x48:
  case 0x84:
    return true;
  default:
    return false;
  }
}

vec3<int16_t> diagonal_position(vec3<int16_t> const &source) {
  // Native 0xb7858's optional 45-degree box transform deliberately wraps each product to 32 bits before dividing towards zero.
  auto const component{[](int const value){
    auto const product{std::bit_cast<int32_t>(static_cast<uint32_t>(value) * static_cast<uint32_t>(-11585))};
    return math::signed_word(static_cast<uint16_t>(product / 16384));
  }};
  return {component(source.z + source.x), source.y, component(source.y - source.x)};
}

} // anonymous namespace

bool intersects(collision_box const &box, collision_segment const &movement, bool const diagonal) {
  /// Native 0xb7768 requires intersection in all three projections; this is deliberately not a replacement 3D slab algorithm.
  auto const a{diagonal ? diagonal_position(movement.previous) : vec3<int16_t>{movement.previous}};
  auto const b{diagonal ? diagonal_position(movement.current) : vec3<int16_t>{movement.current}};
  auto const &lo{box.minimum};
  auto const &hi{box.maximum};
  return crosses_rectangle({lo.x, lo.z}, {hi.x, hi.z}, {a.x, a.z}, {b.x, b.z})
    && crosses_rectangle({lo.x, lo.y}, {hi.x, hi.y}, {a.x, a.y}, {b.x, b.y})
    && crosses_rectangle({lo.z, lo.y}, {hi.z, hi.y}, {a.z, a.y}, {b.z, b.y});
}

} // namespace tfx::world
