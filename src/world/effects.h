#pragma once

#include <array>
#include <ranges>
#include <string_view>
#include "math/native.h"

namespace tfx::world {

enum class effect_kind : uint16_t { hit, miss, splash, boom, clatter, cannon_splash, cannon_hit, cannon_miss, crash, fireball };
unsigned int constexpr effect_capacity{100}, effect_kind_count{10};
using effect_slot = unsigned int;
std::array<std::string_view,effect_kind_count> constexpr effect_definitions{
  "hit","miss","splash","boom","clatter","can_spsh","can_hit","can_miss","crash","fireball"
};
struct effect_record {
  vec3<int16_t> position{0,0,0};
  effect_kind kind{effect_kind::hit};
};
class effect_pool {
private:
  std::array<effect_record,effect_capacity> records;
  effect_slot next{effect_capacity - 1};
  unsigned int count{0};

public:
  void reset();
  effect_slot spawn(vec3<int32_t> const &position, effect_kind kind);
  effect_record const &record(effect_slot slot) const;
  auto active_slots() const {
    // Native 0x7b144 appends to an oldest-first chain. Full pools reuse its head.
    return std::views::iota(0u,count) | std::views::transform([this](unsigned int const index){ return (next + count - index) % effect_capacity; });
  }
};

// Place a word-positioned native effect in the nearest 65,536-unit world copy.
vec3<int32_t> effect_position(vec3<int16_t> const &position, vec3<int32_t> const &reference);

} // namespace tfx::world
