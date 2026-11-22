#pragma once

#include "flight/stores.h"
#include "resources/binary.h"

namespace tfx::flight {

struct airframe {
  airframe_load stores;
  std::array<vec3d, store_slots> mounts;
};
std::array<airframe, 3> load_airframes(std::span<uint8_t const> executable);

} // namespace tfx::flight
