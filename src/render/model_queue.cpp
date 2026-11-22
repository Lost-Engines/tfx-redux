#include "model_queue.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tfx::render {

model_slot_state model_slot_metadata::queued(model_slot const slot, model_render_path const path, uint8_t const scale, vec3<math::angle> const &angles) {
  auto &state{slots.at(slot)};
  state.path = path;
  if(path != model_render_path::unrotated) {
    state.scale = scale;
    state.angles = vec3<math::angle>{angles};
  }
  return state;
}
model_slot_state model_slot_metadata::immediate(model_slot const slot, uint8_t const scale, vec3<math::angle> const &angles) {
  auto &state{slots.at(slot)};
  state.scale = scale;
  state.angles = vec3<math::angle>{angles};
  return state;
}
void model_slot_metadata::restore(model_slot const slot, model_slot_state const &state) {
  slots.at(slot) = state;
}

void model_queue::clear() {
  count = 0;
  current = 0;
  sorted = false;
}
unsigned int model_queue::size() const {
  return count;
}
std::span<model_queue_entry const> model_queue::pending() const {
  return std::span{entries}.first(count);
}
void model_queue::restore(std::span<model_queue_entry const> const source) {
  if(source.size() > model_queue_capacity) throw std::runtime_error{"recorded model queue exceeds its native capacity"};
  for(auto const &entry : source) {
    if(entry.slot >= source.size()) throw std::runtime_error{"recorded model queue refers to an absent slot"};
  }
  clear();
  count = static_cast<unsigned int>(source.size());
  std::copy(source.begin(), source.end(), entries.begin());
  current = static_cast<model_slot>(std::min(count, model_queue_capacity - 1));
}
model_slot model_queue::scratch_slot() const {
  return current;
}

void model_queue::insertion_sort() {
  for(unsigned int i{1}; i < count; ++i) {
    entry const value{entries[i]};
    unsigned int at{i};
    // Native C4824 moves equal priorities too. Reverse traversal consequently
    // retains its insertion/tie behaviour, including repeated full-queue sorts.
    // The explicit bound also safely handles the original sentinel value.
    while(at && value.priority <= entries[at - 1].priority) {
      entries[at] = entries[at - 1];
      --at;
    }
    entries[at] = value;
  }
}
void model_queue::quick_sort(int const first, int const last) {
  // Original one-based partition bounds and last-element pivot. Small runs
  // remain for the final insertion pass; changing that changes equal keys.
  int constexpr insertion_run{4};
  if(last - first <= insertion_run) return;
  auto const pivot{entries[static_cast<unsigned int>(last - 1)].priority};
  int left{first - 2};
  int right{last - 1};
  for(;;) {
    do {
      ++left;
    } while(pivot > entries[static_cast<unsigned int>(left)].priority);
    do {
      --right;
    } while(right >= 0 && pivot < entries[static_cast<unsigned int>(right)].priority);
    if(left >= right) break;
    std::swap(entries[static_cast<unsigned int>(left)], entries[static_cast<unsigned int>(right)]);
  }
  std::swap(entries[static_cast<unsigned int>(left)], entries[static_cast<unsigned int>(last - 1)]);
  quick_sort(first, left);
  quick_sort(left + 2, last);
}
model_slot model_queue::insert(int16_t const priority) {
  model_slot const slot{current};
  if(++count < model_queue_capacity) {
    entries[current] = {
      .priority{priority},
      .slot{current},
    };
    current = static_cast<model_slot>(count);
    return slot;
  }
  if(sorted) {
    entries.back().priority = priority;
  } else {
    entries[current] = {
      .priority{priority},
      .slot{static_cast<model_slot>(model_queue_capacity - 1)},
    };
  }
  count = model_queue_capacity;
  // C4930 switches its own branch after the first fill. Later insertions
  // replace the previous last entry and reuse that entry's payload slot.
  insertion_sort();
  current = entries.back().slot;
  sorted = true;
  return slot;
}
std::vector<model_slot> model_queue::draw_order() {
  if(!sorted) {
    unsigned int constexpr quick_sort_threshold{15};
    if(count >= quick_sort_threshold) quick_sort(1, static_cast<int>(count));
    if(count >= 2) {
      insertion_sort();
      sorted = true;
    }
  }
  std::vector<model_slot> result;
  result.reserve(count);
  for(unsigned int index{count}; index; --index) result.push_back(entries[index - 1].slot);
  return result;
}

} // namespace tfx::render
