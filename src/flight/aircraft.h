#pragma once

namespace tfx::flight {

// Mission-script values; the native simulation uses the corresponding zero-based index.
enum class player_aircraft : unsigned int {
  eurofighter = 1,
  nighthawk,
  raptor
};

} // namespace tfx::flight
