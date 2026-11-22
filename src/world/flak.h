#pragma once

#include <array>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include "missions/initialisation.h"

namespace tfx::world {

unsigned int constexpr flak_capacity{20}, flak_night_models{5};
int16_t constexpr initial_flak_delay{5};
std::array<std::string_view, flak_night_models> constexpr night_flak_definitions{"snake_1", "snake_2", "snake_3", "snake_4", "snake_5"};
std::string_view constexpr day_flak_definition{"burst"};
struct flak_random_state {
  uint16_t phase{0xac57};
  uint16_t scatter{0x6b5a};
};
struct flak_state {
  int16_t delay{initial_flak_delay};
  flak_random_state random{};
  uint8_t night_model{0};
  vec2<int32_t> centre{0, 0};
};
struct flak_context {
  vec3<int32_t> player;
  std::span<missions::flak_region const> regions;
  bool night;
  bool cloud_layer;
  int16_t layer_y;
  bool radio_idle{true};
};
struct flak_event {
  vec3<int32_t> position;
  uint8_t model;
  bool sample;
  bool radio;
  bool flash;
};
std::optional<flak_event> advance_flak(flak_state &state, flak_context const &context);
bool flak_visible(flak_state const &state, vec3<int32_t> const &player);

struct flak_record {
  vec3<int16_t> position{0, 0, 0};
  uint8_t model{0};
};
class flak_pool {
private:
  std::array<flak_record, flak_capacity> records;
  unsigned int next{flak_capacity - 1};
  unsigned int count{0};

public:
  void reset();
  unsigned int spawn(flak_event const &event);
  flak_record const &record(unsigned int slot) const;
  auto active_slots() const {
    return std::views::iota(0u, count) | std::views::transform([this](unsigned int const index){return (next + count - index) % flak_capacity;});
  }
};

} // namespace tfx::world
