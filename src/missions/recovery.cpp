#include "recovery.h"
#include <utility>
#include "resources/media.h"
#include "world/environment.h"

namespace tfx::missions {

recovery_region pilot_recovery_region(std::string_view const world) {
  if(world == "columbia") return recovery_region::columbia;
  if(world == "somalia") return recovery_region::somalia;
  if(world == "libya") return recovery_region::libya;
  if(world == "yugoslavia" || world == "yugoslav") return recovery_region::yugoslavia;
  if(world == "seaworld") return recovery_region::atlantic;
  return recovery_region::unknown;
}
recovery_maps::recovery_maps(resources::archive const &game) {
  std::array<char const *, 6> constexpr names{"capsom", "capcol", "caplib", "capyug", "capsea", "noncap"};
  for(unsigned int index{0}; index != names.size(); ++index) {
    auto const name{std::string{"iff/raw/"} + names[index] + ".lbm"};
    // The original names a fallback noncap.lbm that is absent from retail 2.05.
    // Load only available maps; ordinary missions always select one of the five regions.
    if(game.contains(name)) maps[index].emplace(resources::decode_screen(game.read(name)).image);
  }
}
recovery_state recovery_maps::resolve(recovery_state result, recovery_context const &context) const {
  // Native launcher 0x72a21 skips this override when the pilot impact controller
  // has already committed a fatal result. Violations bypass the capture map.
  if(!context.ejected || context.fatal) return result;
  if(context.violations) {
    result.outcome = session_outcome::violated_orders;
    result.status = session_status::violated_orders;
    return result;
  }
  result.outcome = session_outcome::ejected;
  result.status = context.damaged ? session_status::normal : session_status::violated_orders;
  auto const &map{maps.at(std::to_underlying(context.region))};
  if(!map) throw std::runtime_error{"pilot recovery map unavailable for this region"};
  auto const point{[](vec2<int32_t> const &coordinates){
    // Native uses truncating division, then stores both pixel coordinates in words.
    return vec2<int16_t>{math::signed_word(static_cast<uint16_t>(coordinates.x / world::cell_units)), math::signed_word(static_cast<uint16_t>(world::map_height - 1 - coordinates.y / world::cell_units))};
  }};
  auto const start{point(context.start)};
  auto const position{point(context.position)};
  bool missing{true};
  if(position.x >= 0 && position.x < render::screen_width && position.y >= 0 && position.y < render::screen_height) {
    // Original 0x72424 first marks the starting cell with palette index 1.
    // That overrides the capture map when the player lands back on that cell.
    uint8_t constexpr hostile_area{2};
    missing = position != start && map->pixels[static_cast<unsigned int>(position.y * render::screen_width + position.x)] == hostile_area;
  }
  result.safe = !missing;
  if(missing) result.status = session_status::missing;
  return result;
}

} // namespace tfx::missions
