#pragma once

#include "presentation/fonts.h"
#include "presentation/pilots.h"
#include "presentation/timing.h"
#include "resources/archive.h"

namespace tfx::presentation {

class pilot_panel {
private:
  enum class page : uint8_t {
    choice,
    names,
    load_slots,
    save_slots,
    overwrite,
    discard,
    leaving,
    complete
  } current{page::choice};
  fonts const &font_data;
  pilot_assets const &assets;
  program const &schema;
  variables &state;
  pilot_store const &store;
  resources::screen_image dialogue;
  resources::screen_image slots_image;
  dac_palette dialogue_palette{};
  dac_palette slots_palette{};
  render::framebuffer display;
  std::array<std::optional<saved_state>, pilot_slots> records;
  std::optional<name_entry> editing;
  std::optional<saved_state> loaded;
  unsigned int selected{0};
  unsigned int name_field{0};
  unsigned int ramp{0};
  bool entering{true};
  bool fixed_slot{false};
  bool slot_background{false};
  std::optional<unsigned int> destination;
  static std::array<std::string_view, 3> constexpr name_variables{"firstname", "pilotname", "callsign"};
  void begin_names();
  void open_slots(bool load);
  void choose_slot();
  void finish();
  void draw(std::optional<vec2<int16_t>> const &position);
  void centred(std::string_view text, int y, uint8_t colour, font_id font = 9);

public:
  pilot_panel(resources::archive const &game, fonts const &fonts, pilot_assets const &assets,
    program const &schema, variables &state, pilot_store const &store);
  void press(uint8_t scan, bool control = false);
  void click(vec2<int16_t> const &position);
  bool advance(std::optional<vec2<int16_t>> const &position = {});
  render::framebuffer const &image() const;
  render::palette colours() const;
  unsigned int slot() const;
  std::optional<saved_state> const &recalled() const;
};

} // namespace tfx::presentation
