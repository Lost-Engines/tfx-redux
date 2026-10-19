#include "pilot_panel.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "presentation/timing.h"

namespace tfx::presentation {
namespace {
uint32_t number(variables const &values, std::string_view const name) {
  auto const found{values.find(name)};
  return found == values.end() ? 0 : std::get<uint32_t>(found->second);
}
std::string text(variables const &values, std::string_view const name) {
  auto const found{values.find(name)};
  return found == values.end() ? std::string{} : std::get<std::string>(found->second);
}
} // anonymous namespace

pilot_panel::pilot_panel(resources::archive const &game, fonts const &fonts, pilot_assets const &assets,
  program const &schema, variables &state, pilot_store const &store) : font_data{fonts}, assets{assets}, schema{schema}, state{state}, store{store},
  dialogue{resources::decode_screen(game.read("iff/raw/com_blk3.lbm"))}, slots_image{resources::decode_screen(game.read("iff/raw/com_enrl.lbm"))}, records{store.list(schema)} {
  auto const read_palette{[&](std::string_view const name, dac_palette &target) {
    auto const bytes{game.read(name)};
    std::ranges::copy(std::span{bytes}.subspan(4,target.size()),target.begin());
  }};
  read_palette("iff/raw/com_blk3.lbm",dialogue_palette);
  read_palette("iff/raw/com_enrl.lbm",slots_palette);
  draw({});
}
void pilot_panel::begin_names() {
  reset_pilot(schema,state);
  current = page::names;
  slot_background = false;
  name_field = 0;
  editing.emplace(assets,assets.defaults[0]);
}
void pilot_panel::open_slots(bool const load) {
  current = load ? page::load_slots : page::save_slots;
  slot_background = true;
  selected = 0;
  records = store.list(schema);
}
void pilot_panel::finish() {
  current = page::leaving;
  ramp = 40;
}
void pilot_panel::choose_slot() {
  destination = selected;
  if(current == page::load_slots) {
    if(!records[selected]) { fixed_slot = true;begin_names(); }
    else if(number(records[selected]->values,"status") != 0) current = page::discard;
    else { loaded = records[selected];finish(); }
  } else if(records[selected] && number(records[selected]->values,"status") == 0) current = page::overwrite;
  else finish();
}
void pilot_panel::press(uint8_t const scan, bool const control) {
  if(entering || current == page::leaving || current == page::complete) return;
  if(current == page::names) {
    if(editing->press(scan,control)) {
      state[std::string{name_variables[name_field]}] = editing->value();
      if(++name_field == name_variables.size()) {
        state["rank"] = uint32_t{editing->qualified() || store.trained()};
        editing.reset();
        if(fixed_slot) finish();
        else open_slots(false);
      } else editing.emplace(assets,assets.defaults[name_field]);
    }
    return;
  }
  if(current == page::overwrite || current == page::discard) {
    if(scan == 0x15) {
      if(current == page::overwrite) finish();
      else { records[selected].reset();current = page::load_slots; }
    } else if(scan == 0x31) current = current == page::overwrite ? page::save_slots : page::load_slots;
    return;
  }
  unsigned int const count{current == page::choice ? 2u : pilot_slots};
  if(scan == 0x48 && selected) --selected;
  else if(scan == 0x50 && selected + 1 < count) ++selected;
  else if(scan == 0x1c) {
    if(current == page::choice) {
      if(selected == 0) begin_names();
      else open_slots(true);
    } else choose_slot();
  }
}
void pilot_panel::click(vec2<int16_t> const &position) {
  if(entering) return;
  if(current == page::choice) {
    // Original 0xa1c44 reuses the final option's half-width for both click rows.
    int const half{font_data.measure(9,"LOAD PREVIOUS GAME") / 2};
    for(unsigned int row{0}; row != 2; ++row) if(position.x >= 160 - half && position.x <= 160 + half && position.y >= 90 + static_cast<int>(row) * 10 && position.y <= 96 + static_cast<int>(row) * 10) {
      selected = row;
      press(0x1c);
      return;
    }
  } else if(current == page::load_slots || current == page::save_slots) {
    for(unsigned int row{0}; row != pilot_slots; ++row) if(position.x >= 25 && position.x <= 315 && position.y >= 45 + static_cast<int>(row) * 8 && position.y <= 50 + static_cast<int>(row) * 8) {
      if(selected == row) choose_slot();
      else selected = row;
      return;
    }
  }
}
void pilot_panel::centred(std::string_view const text, int const y, uint8_t const colour, font_id const font) {
  font_data.draw(display,font,{160 - font_data.measure(font,text) / 2,y},text,colour);
}
void pilot_panel::draw(std::optional<vec2<int16_t>> const &position) {
  bool const slots{current == page::load_slots || current == page::save_slots || current == page::overwrite || current == page::discard};
  display = slot_background ? slots_image.image : dialogue.image;
  if(current == page::choice) {
    std::array<std::string_view,2> constexpr options{"CREATE NEW PILOT","LOAD PREVIOUS GAME"};
    for(unsigned int row{0}; row != options.size(); ++row) {
      int const y{90 + static_cast<int>(row) * 10};
      if(row == selected) {
        int const half{font_data.measure(9,options[row]) / 2};
        for(int py{y - 2}; py <= y + 7; ++py) for(int px{158 - half}; px <= 162 + half; ++px) display.write(static_cast<unsigned int>(py * 320 + px),14);
      }
      centred(options[row],y,row == selected ? 159 : 215);
    }
  } else if(current == page::names) {
    for(unsigned int row{0}; row != assets.introduction.size(); ++row) centred(assets.introduction[row].text,10 + static_cast<int>(row) * 10,assets.introduction[row].colour);
    for(unsigned int index{0}; index != name_field; ++index) centred(text(state,name_variables[index]),120 + static_cast<int>(index) * 30,215);
    int const y{120 + static_cast<int>(name_field) * 30};
    for(int py{y - 2}; py <= y + 7; ++py) for(int px{80}; px <= 240; ++px) display.write(static_cast<unsigned int>(py * 320 + px),14);
    centred(editing->value(),y,215);
  } else if(slots) {
    // Native slot rows and prompts use the small-font painters D2AFC/D2B54.
    font_id constexpr slot_font{6};
    font_data.draw(display,slot_font,{106,14},current == page::load_slots || current == page::discard ? "CHOOSE GAME SLOT TO LOAD" : "CHOOSE SAVE GAME SLOT",223);
    for(unsigned int row{0}; row != records.size(); ++row) {
      auto const &record{records[row]};
      std::string const status{record ? assets.statuses.at(number(record->values,"status")) : "-----"};
      std::string const rank{record ? assets.ranks.at(number(record->values,"rank")) : "----"};
      std::string const name{record ? text(record->values,"firstname") + " -" + text(record->values,"callsign") + "- " + text(record->values,"pilotname") : "BLANK"};
      int const y{45 + static_cast<int>(row) * 8};
      uint8_t const colour{static_cast<uint8_t>(row == selected ? 156 : 49)};
      for(auto const &item : {std::pair{55 - font_data.measure(slot_font,status),status},std::pair{71,rank},std::pair{106,name}}) {
        if(row == selected) font_data.draw(display,slot_font,{item.first + 1,y + 1},item.second,0);
        font_data.draw(display,slot_font,{item.first,y},item.second,colour);
      }
    }
    std::array<std::string_view,5> constexpr statistics{"score","a2akills","a2gkills","num_miss","num_hours"};
    for(unsigned int index{0}; index != statistics.size(); ++index) {
      auto const value{records[selected] ? std::to_string(std::bit_cast<int32_t>(number(records[selected]->values,statistics[index]))) : "--"};
      font_data.draw(display,slot_font,{85 - font_data.measure(slot_font,value),147 + static_cast<int>(index) * 8},value,55);
    }
    if(current == page::overwrite || current == page::discard) {
      std::string_view const prompt{current == page::overwrite ? "THIS PILOT IS IN ACTIVE DUTY   OVERWRITE ENTRY Y - N" : "THIS PILOT IS NO LONGER IN ACTIVE DUTY   DELETE Y - N"};
      font_data.draw(display,slot_font,{107,135},prompt,0);
      font_data.draw(display,slot_font,{106,134},prompt,156);
    }
  }
  if(position && current != page::names) draw_pointer(display,assets.pointer,*position);
}
bool pilot_panel::advance(std::optional<vec2<int16_t>> const &position) {
  if(current == page::complete) return true;
  if(current == page::leaving) {
    if(ramp) --ramp;
    if(!ramp) { current = page::complete;display.pixels.fill(0);return true; }
    return false;
  }
  draw(position);
  if(entering && ++ramp == 40) entering = false;
  return false;
}
render::framebuffer const &pilot_panel::image() const {
  return display;
}
render::palette pilot_panel::colours() const {
  return render::decode_palette(fade_palette(slot_background ? slots_palette : dialogue_palette,ramp,40));
}
unsigned int pilot_panel::slot() const {
  if(!destination) throw std::runtime_error{"pilot panel has no selected storage slot"};
  return *destination;
}
std::optional<saved_state> const &pilot_panel::recalled() const {
  return loaded;
}

} // namespace tfx::presentation
