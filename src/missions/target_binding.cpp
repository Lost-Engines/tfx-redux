#include "target_binding.h"
#include <bit>
#include <optional>
#include <stdexcept>
#include <utility>
#include "actors/surface.h"

namespace tfx::missions {

target_binding bind_target(vec3<int32_t> const &position,
    std::span<vec3<int32_t> const> const fixed,
    std::span<surface_target_candidate const> const surface) {
  vec2<int32_t> const cell{
    position.x >> world::cell_shift,
    position.z >> world::cell_shift,
  };
  unsigned int constexpr actor_cell_shift{world::cell_shift + actors::position_fraction_bits};
  uint32_t constexpr half_cell{uint32_t{1} << (actor_cell_shift - 1)};
  auto const centre{[](int16_t const value){
    return std::bit_cast<int32_t>((static_cast<uint32_t>(value) << actor_cell_shift) + half_cell);
  }};
  // Actor searches truncate cells to signed words; terrain retains dwords.
  vec2<int32_t> const point{
    centre(math::signed_word(static_cast<uint16_t>(cell.x))),
    centre(math::signed_word(static_cast<uint16_t>(cell.y))),
  };
  auto const matches{[&](vec3<int32_t> const &candidate){
    return candidate.x == point.x && candidate.z == point.y;
  }};
  for(unsigned int index{0}; index != fixed.size(); ++index)
    if(matches(fixed[index])) return fixed_target{
      .index{index},
    };
  std::optional<unsigned int> slot;
  auto constexpr excluded{std::to_underlying(actors::surface_flag::destroyed) | std::to_underlying(actors::surface_flag::retired)};
  for(auto const &candidate : surface) {
    if(candidate.slot >= actors::surface_capacity) throw std::out_of_range{"mission target surface slot exceeds its registry"};
    if(!(candidate.flags & excluded) && matches(candidate.position_q8) && (!slot || candidate.slot < *slot)) slot = candidate.slot;
  }
  if(slot) return surface_target{
    .slot{*slot},
  };
  return terrain_target{
    .cell{vec2<int32_t>{cell}},
  };
}

} // namespace tfx::missions
