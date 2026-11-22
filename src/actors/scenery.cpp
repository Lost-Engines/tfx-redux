#include "scenery.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::actors {

std::vector<scenery_placement> initialise_scenery(program const &source, std::span<profile const> const profiles) {
  std::vector<scenery_placement> result;
  for(unsigned int index{0}; index != source.records.size(); ++index) {
    auto const &record{source.records[index]};
    if(record.kind != record_kind::scenery) continue;
    resources::binary_view const body{record.body};
    uint16_t const id{body.le16(13)};
    auto const found{std::ranges::find(profiles, id, &profile::id)};
    if(found == profiles.end()) throw std::runtime_error{"fixed scenery references an unknown profile"};
    result.push_back({
      .record{index},
      .profile_id{id},
      .definition{found->definition},
      .position{std::bit_cast<int32_t>(body.le32(1)), std::bit_cast<int32_t>(body.le32(5)), std::bit_cast<int32_t>(body.le32(9))},
      .event{body.le16(19)},
    });
  }
  // Native 0x75628 prepends each placement to a linked list; collision/drawing use reverse authored order.
  std::ranges::reverse(result);
  return result;
}

} // namespace tfx::actors
