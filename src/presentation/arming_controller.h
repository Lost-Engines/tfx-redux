#pragma once

#include "presentation/arming_assets.h"
#include "weapons/loadout.h"

namespace tfx::presentation {

enum class arming_mode { idle, rotating, placing, clearing, overview, unavailable, scrolling, complete, mission_incomplete, confirm_return, cancelled };
struct arming_state {
  arming_mode mode{arming_mode::idle};
  unsigned int first_store{1};
  std::optional<weapons::weapon_id> selected{};
  vec2<math::angle> angles{0x14f0,0x7130};
  uint8_t pulse{0};
};

class arming_controller {
private:
  weapons::armament &inventory;
  arming_assets const &assets;
  unsigned int default_fit;
  arming_entry entry;
  arming_state current;
  std::array<bool,flight::store_slots> preview_slots{};
  bool held{false};
  void remove_previews();

public:
  arming_controller(weapons::armament &inventory, arming_assets const &assets, unsigned int default_fit, arming_entry entry = arming_entry::normal);
  ~arming_controller();
  arming_controller(arming_controller const&) = delete;
  arming_controller &operator=(arming_controller const&) = delete;
  arming_state const &state() const;
  std::span<bool const> previews() const;
  std::optional<arming_region> hit(vec2<int16_t> const &position) const;
  void choose(weapons::weapon_id weapon);
  void place(unsigned int slot);
  void clear(unsigned int slot);
  void cancel();
  void finish(bool escape);
  void press(uint8_t scan);
  void advance(vec2<int16_t> const &position, vec2<int16_t> const &movement, uint8_t buttons, std::optional<unsigned int> picked = {});
};

} // namespace tfx::presentation
