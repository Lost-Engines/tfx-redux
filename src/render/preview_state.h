#pragma once

#include <optional>

namespace tfx::render {

// Render a frozen copy of a stateful original painter/controller. Ordinary
// frames use the live object without copying; paused frames cannot advance it.
template<class State> class preview_state {
  State &source;
  std::optional<State> copy;
public:
  preview_state(State &source, bool frozen) : source{source} {
    if(frozen) copy.emplace(source);
  }
  State &value() { return copy ? *copy : source; }
  State *operator->() { return &value(); }
};

} // namespace tfx::render
