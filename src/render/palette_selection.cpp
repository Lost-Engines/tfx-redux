#include "palette_selection.h"

namespace tfx::render {

palette_selection::palette_selection(palette_selection_state const initial) : state{initial} {}
palette_selection_state const &palette_selection::value() const {
  return state;
}
void palette_selection::invalidate_normal() {
  state.normal_changed = true;
}
std::optional<palette_bank> palette_selection::advance(palette_context const &context) {
  using cockpit::view_mode;
  // Native 0x73b76: the nose view tests the entire active projectile list,
  // independently of the selected store's guidance/infrared capabilities.
  bool infrared{context.view == view_mode::designator || (context.view == view_mode::missile_infrared && context.projectiles_present)};
  if(!infrared && context.infrared_available && context.night && context.night_vision) {
    switch(context.view) {
    case view_mode::chase:
    case view_mode::unused_5:
    case view_mode::missile:
    case view_mode::tactical:
    case view_mode::approach_cockpit:
    case view_mode::depart_cockpit:
    case view_mode::fly_past:
    case view_mode::ejection:
    case view_mode::turning_recovery:
    case view_mode::impact_orbit:
      break;
    default:
      infrared = true;
    }
  }
  if(infrared) {
    if(state.infrared) return std::nullopt;
    state.infrared = true;
    // A changed normal palette remains pending while infrared is displayed.
    return palette_bank::infrared;
  }
  if(!state.normal_changed && !state.infrared) return std::nullopt;
  state.normal_changed = false;
  state.infrared = false;
  return palette_bank::normal;
}

} // namespace tfx::render
