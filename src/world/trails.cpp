#include "trails.h"
#include <stdexcept>
#include <cmath>
#include "world/effects.h"

namespace tfx::world {

vec3<int32_t> trail_segment::midpoint() const {
  // Native 0x7b7fc averages sign-extended endpoint words with SAR rounding.
  auto const sum{vec3<int32_t>{previous} + vec3<int32_t>{current}};
  return {sum.x >> 1,sum.y >> 1,sum.z >> 1};
}
std::optional<trail_segment> prepare_trail(vec3<int32_t> const &previous, vec3<int32_t> const &current, bool const enabled) {
  if(!enabled) return std::nullopt;
  vec3<int16_t> const begin{previous}, end{current};
  int16_t const length{math::distance_word(end - begin)};
  if(!length) return std::nullopt;
  return trail_segment{
    .previous{vec3<int16_t>{begin}},
    .current{vec3<int16_t>{end}},
    .length{length},
  };
}
void emit_trail(trail_segment const &segment, std::function_ref<void(vec3<int32_t> const&)> output) {
  if(!segment.length) throw std::invalid_argument{"zero-length trail segment"};
  vec3<int32_t> const delta{segment.current - segment.previous};
  int constexpr step_scale{trail_spacing * actors::position_unit};
  // Three signed IDIV operations use the signed low-word length, including
  // wrapped long segments. The loop comparison is also a signed word.
  vec3<int32_t> const step{delta * step_scale / static_cast<int32_t>(segment.length)};
  vec3<int32_t> point{vec3<int32_t>{segment.previous} * actors::position_unit};
  int16_t remaining{segment.length};
  do {
    output(point);
    point += step;
    remaining = math::signed_word(static_cast<uint16_t>(remaining - trail_spacing));
  } while(remaining >= trail_spacing);
}

void trail_sampler::advance(vec3<int32_t> const &previous_q8, vec3<int32_t> const &current_q8,
                            std::function_ref<void(vec3<int32_t> const&)> output) {
  auto const start{vec3d{previous_q8}}, delta{vec3d{current_q8} - start};
  double const length{std::sqrt(delta.dot(delta))};
  if(length == 0) return;
  double const spacing{trail_spacing * actors::position_unit};
  // Exclude the endpoint so splitting a segment cannot duplicate a sample.
  for(; until_next < length; until_next += spacing) {
    auto const point{start + delta * (until_next / length)};
    output({static_cast<int32_t>(std::lround(point.x)),static_cast<int32_t>(std::lround(point.y)),static_cast<int32_t>(std::lround(point.z))});
  }
  until_next -= length;
}

void trail_pool::reset() { first = 0;next = 0;count = 0; }
trail_slot trail_pool::spawn(vec3<int32_t> const &position_q8) {
  if(count == trail_capacity) retire_oldest();
  auto const slot{next};
  records[slot].position_q8 = vec3<int32_t>{position_q8};
  next = (next + 1) % trail_capacity;
  ++count;
  return slot;
}
void trail_pool::retire_oldest() {
  if(!count) throw std::out_of_range{"empty smoke-trail pool"};
  first = (first + 1) % trail_capacity;
  --count;
}
void trail_pool::clean(vec3<int32_t> const &reference, std::function_ref<bool(trail_slot)> destroyed, bool const limit_distance) {
  vec3<int16_t> const origin{reference};
  while(count) {
    if(!destroyed(first)) {
      if(!limit_distance) break;
      auto const &p{records[first].position_q8};
      vec3<int16_t> const point{
        math::signed_word(static_cast<uint16_t>(p.x >> actors::position_fraction_bits)),
        math::signed_word(static_cast<uint16_t>(p.y >> actors::position_fraction_bits)),
        math::signed_word(static_cast<uint16_t>(p.z >> actors::position_fraction_bits)),
      };
      if(static_cast<uint16_t>(math::distance_word(point - origin)) <= maximum_trail_distance) break;
    }
    retire_oldest(); // Native 0x7b744 retires only a consecutive dead/distant prefix.
  }
}
trail_record const &trail_pool::record(trail_slot const slot) const { return records.at(slot); }

vec3d trail_position(vec3<int32_t> const &position_q8, vec3<int32_t> const &reference) {
  vec3<int16_t> const whole{
    math::signed_word(static_cast<uint16_t>(position_q8.x >> actors::position_fraction_bits)),
    math::signed_word(static_cast<uint16_t>(position_q8.y >> actors::position_fraction_bits)),
    math::signed_word(static_cast<uint16_t>(position_q8.z >> actors::position_fraction_bits)),
  };
  unsigned int constexpr fraction_mask{actors::position_unit - 1};
  vec3d const fraction{
    static_cast<double>(static_cast<uint32_t>(position_q8.x) & fraction_mask),
    static_cast<double>(static_cast<uint32_t>(position_q8.y) & fraction_mask),
    static_cast<double>(static_cast<uint32_t>(position_q8.z) & fraction_mask),
  };
  return vec3d{effect_position(whole,reference)} + fraction / actors::position_unit;
}

} // namespace tfx::world
