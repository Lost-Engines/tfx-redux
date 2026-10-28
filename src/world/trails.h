#pragma once

#include <array>
#include <functional>
#include <optional>
#include <ranges>
#include <string_view>
#include "actors/program.h"

namespace tfx::world {

unsigned int constexpr trail_capacity{200};
int constexpr trail_spacing{10}, maximum_trail_distance{8000};
std::string_view constexpr trail_definition{"smokecld"};

struct trail_segment {
  vec3<int16_t> previous;
  vec3<int16_t> current;
  int16_t length;
  vec3<int32_t> midpoint() const;
};
std::optional<trail_segment> prepare_trail(vec3<int32_t> const &previous, vec3<int32_t> const &current, bool enabled);
void emit_trail(trail_segment const &segment, std::function_ref<void(vec3<int32_t> const&)> output);

// Redux: retain fractional spacing across projectile updates, independent of
// how many simulation ticks a rendered frame took.
class trail_sampler {
  double until_next{0};
public:
  void advance(vec3<int32_t> const &previous_q8, vec3<int32_t> const &current_q8,
               std::function_ref<void(vec3<int32_t> const&)> output);
};

using trail_slot = unsigned int;
struct trail_record {
  vec3<int32_t> position_q8{0,0,0};
};
class trail_pool {
private:
  std::array<trail_record,trail_capacity> records;
  trail_slot first{0}, next{0};
  unsigned int count{0};

public:
  void reset();
  trail_slot spawn(vec3<int32_t> const &position_q8);
  void retire_oldest();
  void clean(vec3<int32_t> const &reference, std::function_ref<bool(trail_slot)> destroyed, bool limit_distance = true);
  trail_record const &record(trail_slot slot) const;
  auto active_slots() const {
    return std::views::iota(0u,count) | std::views::transform([this](unsigned int const index) { return (first + index) % trail_capacity; });
  }
};
vec3d trail_position(vec3<int32_t> const &position_q8, vec3<int32_t> const &reference);

} // namespace tfx::world
