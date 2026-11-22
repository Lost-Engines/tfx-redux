#include "manoeuvre.h"
#include <bit>
#include <sstream>
#include <stdexcept>

namespace tfx::actors {

manoeuvre::manoeuvre(resources::bytes bytes) : source{std::move(bytes)} {
  resources::binary_view const view{source};
  view.slice(0, manoeuvre_header_bytes + 1);
  category = view.byte(0);
  entry_angles = vec2<math::angle>{view.le16(2), view.le16(4)};
  minimum_speed = view.signed_le16(6);
  relative_altitude = std::bit_cast<int32_t>(view.le32(8));
  if(category > 2) throw std::runtime_error{"invalid CBN manoeuvre category"};
  unsigned int cursor{manoeuvre_header_bytes};
  while(view.byte(cursor)) {
    view.slice(cursor, manoeuvre_step_bytes);
    if(view.byte(cursor) > 127) throw std::runtime_error{"invalid CBN manoeuvre duration"};
    cursor += manoeuvre_step_bytes;
  }
}

std::vector<manoeuvre> load_manoeuvres(resources::archive const &game) {
  // Native 0x75408 reads CBN.DIR in packed installations; keep its authored bank order.
  auto const bytes{game.read("cbn.dir")};
  std::istringstream list{std::string{bytes.begin(), bytes.end()}};
  std::vector<manoeuvre> result;
  std::array<unsigned int, 3> counts{};
  for(std::string name; list >> name;) {
    manoeuvre sequence{game.read("cbn/" + name)};
    if(counts[sequence.category]++ < manoeuvres_per_bank) result.push_back(std::move(sequence));
  }
  return result;
}

} // namespace tfx::actors
