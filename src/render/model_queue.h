#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "math/attitude.h"

namespace tfx::render {

unsigned int constexpr model_queue_capacity{348};
using model_slot = uint16_t;
enum class model_render_path {
  unrotated,
  scaled,
  ground_shadow
};
struct model_slot_state {
  model_render_path path{model_render_path::unrotated};
  uint8_t scale{255};
  vec3<math::angle> angles{0, 0, 0};
};
// Native constructors partially overwrite persistent slot metadata. CC0BF
// retains size; CBFEC retains the queued type. Queue resets clear neither.
class model_slot_metadata {
private:
  std::array<model_slot_state, model_queue_capacity> slots{};
public:
  model_slot_state queued(model_slot slot, model_render_path path, uint8_t scale, vec3<math::angle> const &angles = {0, 0, 0});
  model_slot_state immediate(model_slot slot, uint8_t scale, vec3<math::angle> const &angles = {0, 0, 0});
  void restore(model_slot slot, model_slot_state const &state);
};

struct model_queue_entry {
  int16_t priority{0};
  model_slot slot{0};
};

// Native C488D/C4930/C48E1: payloads belong to the caller; this owner retains
// the original slot reuse, signed priorities, tie ordering and queue bound.
class model_queue {
private:
  using entry = model_queue_entry;
  std::array<entry, model_queue_capacity> entries{};
  unsigned int count{0};
  model_slot current{0};
  bool sorted{false};
  void quick_sort(int first, int last);
  void insertion_sort();

public:
  void clear();
  model_slot scratch_slot() const;
  model_slot insert(int16_t priority);
  std::vector<model_slot> draw_order();
  unsigned int size() const;
  std::span<model_queue_entry const> pending() const;
  void restore(std::span<model_queue_entry const> source);
};

} // namespace tfx::render
