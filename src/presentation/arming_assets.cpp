#include "arming_assets.h"
#include <stdexcept>

namespace tfx::presentation {
namespace {

arming_action action(unsigned int const file) {
  switch(file) {
  case 0x89210:
    return arming_action::rotate;
  case 0x89590:
    return arming_action::idle;
  case 0x8978c:
    return arming_action::defaults;
  case 0x89658:
    return arming_action::done;
  case 0x89b00:
    return arming_action::clear_all;
  case 0x898ec:
    return arming_action::clear_mount;
  case 0x89488:
    return arming_action::overview;
  case 0x89b2c:
    return arming_action::scroll_up;
  case 0x89b4c:
    return arming_action::scroll_down;
  case 0x89194:
    return arming_action::scroll_thumb;
  default:
    throw std::runtime_error{"unknown original arming control"};
  }
}

} // anonymous namespace

bool arming_region::contains(vec2<int16_t> const &point) const {
  return point.x > minimum.x && point.x < maximum.x && point.y > minimum.y && point.y < maximum.y;
}
arming_assets::arming_assets(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  unsigned int constexpr data{973476};
  unsigned int constexpr code{0x70aa4};
  auto const string{[&](unsigned int const address){
    std::string result;
    for(unsigned int index{0}; index != 128; ++index) {
      auto const byte{source.byte(data + address + index)};
      if(!byte) return result;
      result.push_back(static_cast<char>(byte));
    }
    throw std::runtime_error{"unterminated original arming label"};
  }};
  backdrop = string(0x1cfc);
  for(unsigned int aircraft{0}; aircraft != preview_names.size(); ++aircraft) preview_names[aircraft] = string(source.le32(data + 0x5c00 + aircraft * 4));
  empty_model = string(0x1cf0);
  aircraft_names = {string(0x1d4c), string(0x1d60), string(0x1d58)};
  roles = {string(0x1d68), string(0x1d6c)};
  std::array<unsigned int, 14> constexpr categories_at{0x1d70, 0x1d7c, 0x1d84, 0x1d90, 0x1d98, 0x1da4, 0x1dac, 0x1db8, 0x1da4, 0x1db8, 0, 0x1dc4, 0x1dd8, 0x1dcc};
  for(unsigned int index{0}; index != categories.size(); ++index) {
    if(categories_at[index]) categories[index] = string(categories_at[index]);
  }
  inertia_format = string(0x1d08);
  drag_format = string(0x1d2c);
  // Watcom's formatter silently omits the malformed final percent sign in this original string.
  if(drag_format.ends_with('%')) drag_format.pop_back();
  internal = string(0x1de4);
  maximum = string(0x1df8);
  mass_format = string(0x1e08);
  store_mass_format = string(0x1e2c);
  store_drag_format = string(0x1e34);
  unavailable = {string(0x1c9c), string(0x1cb0)};
  mission_incomplete = string(0x1cc0);
  for(unsigned int corner{0}; corner != unavailable_box.size(); ++corner) unavailable_box[corner] = {source.signed_le16(data + 0x8074 + corner * 4), source.signed_le16(data + 0x8076 + corner * 4)};
  auto const region{[&](unsigned int const at, arming_action const kind, unsigned int const row = 0){
    return arming_region{
      .minimum{source.signed_le16(data + at), source.signed_le16(data + at + 2)},
      .maximum{source.signed_le16(data + at + 4), source.signed_le16(data + at + 6)},
      .action{kind},
      .row{row},
    };
  }};
  for(unsigned int index{0}; index != 11; ++index) {
    unsigned int const at{0x7f44 + index * 12};
    auto const callback{source.le32(data + at + 8)};
    if(!callback) break;
    regions.push_back(region(at, action(callback + code)));
  }
  for(unsigned int row{0}; row != rows.size(); ++row) rows[row] = region(0x7fc8 + row * 12, arming_action::select_store, row);
  for(unsigned int slot{0}; slot != paired.size(); ++slot) {
    paired[slot] = source.le16(data + 0x801c + slot * 2);
    if(paired[slot] >= paired.size()) throw std::runtime_error{"invalid paired arming mount"};
    slot_labels[slot] = {source.signed_le16(data + 0x8030 + slot * 4), source.signed_le16(data + 0x8032 + slot * 4)};
  }
  for(unsigned int corner{0}; corner != scrollbar.size(); ++corner) scrollbar[corner] = {source.signed_le16(data + 0x808c + corner * 4), source.signed_le16(data + 0x808e + corner * 4)};
}

} // namespace tfx::presentation
