#include "arming_controller.h"
#include <algorithm>
#include <utility>

namespace tfx::presentation {

arming_controller::arming_controller(weapons::armament &inventory, arming_assets const &assets, unsigned int const default_fit, arming_entry const entry) : inventory{inventory}, assets{assets}, default_fit{default_fit}, entry{entry} {
  for(unsigned int slot{0}; slot != flight::store_slots; ++slot) inventory.assign(slot,std::nullopt);
  if(entry == arming_entry::mission_incomplete) current.mode = arming_mode::mission_incomplete;
}
arming_controller::~arming_controller() { remove_previews(); }
arming_state const &arming_controller::state() const { return current; }
std::span<bool const> arming_controller::previews() const { return preview_slots; }
void arming_controller::remove_previews() {
  for(unsigned int slot{0}; slot != preview_slots.size(); ++slot) if(std::exchange(preview_slots[slot],false)) inventory.pylons[slot].visible = false;
}
std::optional<arming_region> arming_controller::hit(vec2<int16_t> const &position) const {
  // Use the visible cursor position; retain the last matching authored region.
  vec2<int16_t> const point{position};
  std::optional<arming_region> result;
  for(auto const &region : assets.regions) if(region.contains(point)) result = region;
  for(auto const &region : assets.rows) if(region.contains(point)) result = region;
  return result;
}
void arming_controller::choose(weapons::weapon_id const weapon) {
  remove_previews();
  current.selected = weapon;
  current.mode = arming_mode::unavailable;
  for(unsigned int slot{0}; slot != preview_slots.size(); ++slot) {
    if(inventory.pylons[slot].visible || !inventory.can_fit(slot,weapon)) continue;
    preview_slots[slot] = true;
    inventory.pylons[slot].visible = true;
    current.mode = arming_mode::placing;
  }
}
void arming_controller::place(unsigned int const slot) {
  if(slot >= preview_slots.size() || !current.selected) return;
  for(auto const mount : {slot,assets.paired[slot]}) {
    if(!std::exchange(preview_slots[mount],false)) continue;
    inventory.assign(mount,current.selected);
  }
}
void arming_controller::clear(unsigned int const slot) {
  if(slot >= preview_slots.size()) return;
  for(auto const mount : {slot,assets.paired[slot]}) if(!preview_slots[mount]) inventory.assign(mount,std::nullopt);
}
void arming_controller::cancel() {
  remove_previews();
  current.selected.reset();
  current.mode = arming_mode::idle;
}
void arming_controller::finish(bool const escape) {
  // Escape fits defaults only when the native model table is completely empty.
  // Placement placeholders participate in that test, then are removed on exit.
  if(escape && std::ranges::none_of(std::span{inventory.pylons}.first(flight::store_slots),&weapons::pylon::visible)) inventory.fit_default(default_fit);
  remove_previews();
  current.mode = arming_mode::complete;
}
void arming_controller::press(uint8_t const scan) {
  if(current.mode == arming_mode::complete || current.mode == arming_mode::cancelled) return;
  if(current.mode == arming_mode::confirm_return) {
    if(scan == 0x15 || scan == 0x1c) { cancel();current.mode = arming_mode::cancelled; }
    else if(scan == 1 || scan == 0x31) cancel();
  } else if(scan == 1) {
    if(current.mode == arming_mode::placing || current.mode == arming_mode::clearing || current.mode == arming_mode::rotating || current.mode == arming_mode::overview || current.mode == arming_mode::unavailable) cancel();
    else if(entry == arming_entry::preflight) { cancel();current.mode = arming_mode::cancelled; }
    else if(entry == arming_entry::rearming || entry == arming_entry::mission_incomplete) { cancel();current.mode = arming_mode::confirm_return; }
    else finish(true);
  }
}
void arming_controller::advance(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t const buttons, std::optional<unsigned int> const picked) {
  if(current.mode == arming_mode::complete || current.mode == arming_mode::cancelled) return;
  bool const click{(buttons & 1) && !held};
  held = buttons & 1;
  if(current.mode == arming_mode::confirm_return) {
    if(buttons & 2) cancel();
    else if(click && position.y >= 108 && position.y <= 124) {
      if(position.x >= 95 && position.x < 155) current.mode = arming_mode::cancelled;
      else if(position.x >= 165 && position.x <= 225) cancel();
    }
    return;
  }
  if(buttons & 2) { cancel();return; }
  switch(current.mode) {
  case arming_mode::rotating:
    current.angles += vec2<math::angle>{static_cast<math::angle>(movement.y * 16),static_cast<math::angle>(movement.x * 16)};
    if(click) cancel();
    return;
  case arming_mode::placing:
    if(click && picked) place(*picked);
    return;
  case arming_mode::clearing:
    if(click && picked) clear(*picked);
    return;
  case arming_mode::overview: case arming_mode::unavailable: case arming_mode::mission_incomplete:
    if(click) cancel();
    return;
  case arming_mode::scrolling:
    current.first_store = static_cast<unsigned int>(std::clamp(17 * (position.y - 29) / 121,1,17));
    if(!buttons) cancel();
    return;
  default: break;
  }
  current.selected.reset();
  current.pulse += 8;
  if(!click) return;
  auto const region{hit(position)};
  if(!region) return;
  switch(region->action) {
  case arming_action::rotate: current.mode = arming_mode::rotating;break;
  case arming_action::idle: cancel();break;
  case arming_action::defaults: inventory.fit_default(default_fit);break;
  case arming_action::done: finish(false);break;
  case arming_action::clear_all:
    for(unsigned int slot{0}; slot != flight::store_slots; ++slot) inventory.assign(slot,std::nullopt);
    break;
  case arming_action::clear_mount: current.mode = arming_mode::clearing;break;
  case arming_action::overview: current.mode = arming_mode::overview;break;
  case arming_action::scroll_up: if(current.first_store > 1) --current.first_store;break;
  case arming_action::scroll_down: if(current.first_store < 17) ++current.first_store;break;
  case arming_action::scroll_thumb: current.mode = arming_mode::scrolling;break;
  case arming_action::select_store: choose(static_cast<weapons::weapon_id>(current.first_store + region->row));break;
  }
}

} // namespace tfx::presentation
