#include "executable_tables.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tfx::resources {

executable_tables::executable_tables(std::filesystem::path const &path) {
  /// Read original circle spans as data; no original instructions execute at runtime
  auto const source{read_file(path)};
  uint64_t checksum{14695981039346656037ull};
  for(auto const byte : source) checksum = (checksum ^ byte) * 1099511628211ull;
  if(source.size() != 1032503 || checksum != 0xcc1e305ba7e177f0ull) throw std::runtime_error{"this reconstruction currently requires the September 1994 TFX 2.05 executable (TFX.DAT)"};
  samples = audio::load_sample_definitions(source);
  weapons = weapons::load_definitions(source);
  airframes = flight::load_airframes(source);
  aerodynamic_airframes = flight::load_aerodynamic_airframes(source);
  aerodynamic_wind = flight::load_aerodynamic_wind_parameters(source);
  aerodynamic_temperatures = flight::load_aerodynamic_surface_temperatures(source);
  pointers = presentation::load_pointers(source);
  pilot_assets = std::make_unique<presentation::pilot_assets>(source);
  score_data = std::make_unique<presentation::score_assets>(source);
  arming_data = std::make_unique<presentation::arming_assets>(source);
  instrument_data = std::make_unique<cockpit::instrument_assets>(source);
  refuel_data = std::make_unique<flight::refuel_assets>(source);
  widget_font_data = std::make_unique<presentation::widget_font>(source);
  configuration_data = std::make_unique<presentation::configuration_menu_assets>(source);
  planner_art_data = std::make_unique<planner_artwork>(source);
  font_data = std::make_unique<presentation::fonts>(source);
  binary_view const view{source};
  unsigned int constexpr circle_base{0x70aa4 + 0x65a84}, circle_limit_field{0x70aa4 + 0x66ea0};
  auto const limit{view.le16(circle_limit_field)};
  circles.resize(limit);
  unsigned int constexpr data_file{973476}, mapping{data_file + 0x7a44}, advances{data_file + 0x7a84}, glyph_file{0xf5350};
  for(unsigned int profile{0}; profile != music_orders.size(); ++profile) std::ranges::copy(view.slice(data_file + 0xa38c + profile * 12,12),music_orders[profile].begin());
  std::ranges::copy(view.slice(data_file + 0xd55c,text_palette.size()),text_palette.begin());
  // Native 0x73f5c/0x73964: magenta means leave that palette entry untouched.
  for(unsigned int colour{0}; colour != 256; ++colour) {
    fixed_flight_palette[colour * 3] = 63;
    fixed_flight_palette[colour * 3 + 1] = 0;
    fixed_flight_palette[colour * 3 + 2] = 63;
  }
  unsigned int constexpr fixed_colours{data_file + 0x6e4f};
  for(unsigned int entry{fixed_colours}; view.byte(entry) != 255; entry += 4) {
    unsigned int const colour{view.byte(entry)};
    for(unsigned int channel{0}; channel != 3; ++channel) fixed_flight_palette[colour * 3 + channel] = view.byte(entry + 1 + channel);
  }
  hud_green = view.byte(data_file + 0xa4ec);
  auto const native_message{[&](unsigned int const offset){
    unsigned int constexpr maximum_message_length{512};
    std::string result;
    for(unsigned int index{0}; index != maximum_message_length; ++index) {
      auto const character{view.byte(data_file + offset + index)};
      if(!character) return result;
      result.push_back(static_cast<char>(character));
    }
    throw std::runtime_error{"unterminated native gameplay message"};
  }};
  timer_expired_message = native_message(0xa8);
  fire_permission_message = native_message(0x1850);                            // Both native permission-notice strings have identical text.
  orders_violation_message = native_message(0x678);
  ground_training_warning = native_message(0x370c);
  ground_orders_violation = native_message(0x3764);
  flak_warning = native_message(0x1388);
  for(unsigned int index{0}; index != 64; ++index) {
    auto const type{view.signed_le16(data_file + 0x884c + index * 2)};
    if(type == -1) break;
    ship_target_types.push_back(type);
    if(index == 63) throw std::runtime_error{"unterminated native ship target list"};
  }
  auto const anchor{[&](unsigned int const address){ return vec3<int16_t>{view.signed_le16(data_file + address),view.signed_le16(data_file + address + 2),view.signed_le16(data_file + address + 4)}; }};
  unsigned int constexpr empty_record{data_file + 0x809c + weapons::empty_pylon * 48};
  arming.empty_role = static_cast<weapons::weapon_role>(view.byte(empty_record + 20));
  arming.empty_category = view.byte(empty_record + 29);
  std::array<unsigned int,3> constexpr default_bases{0x7dd0,0x7ec0,0x7e48}, default_counts{12,6,12};
  for(unsigned int aircraft{0}; aircraft != arming.defaults.size(); ++aircraft) {
    arming.defaults[aircraft].resize(default_counts[aircraft]);
    for(unsigned int fit{0}; fit != default_counts[aircraft]; ++fit) for(unsigned int pair{0}; pair != weapons::pylon_pairs; ++pair) arming.defaults[aircraft][fit][pair] = view.signed_le16(data_file + default_bases[aircraft] + (fit * weapons::pylon_pairs + pair) * 2);
  }
  for(unsigned int index{0}; index != arming.next.size(); ++index) arming.next[index] = view.le16(data_file + 0x869c + index * 2);
  for(unsigned int index{0}; index != cannon_mounts.size(); ++index) cannon_mounts[index] = anchor(0x8690 + index * 6);
  std::array<unsigned int,3> constexpr aircraft_mounts{0x8654,0x8618,0x85dc};
  for(unsigned int aircraft{0}; aircraft != store_mounts.size(); ++aircraft) for(unsigned int slot{0}; slot != store_mounts[aircraft].size(); ++slot) store_mounts[aircraft][slot] = anchor(aircraft_mounts[aircraft] + slot * 6);
  // Native 0x8b984 also accepts the empty/cannon sentinels. Their addresses
  // intentionally overlap adjoining stored records; retain those read bytes.
  for(unsigned int aircraft{0}; aircraft != sight_mount_data.size(); ++aircraft) for(unsigned int index{0}; index != sight_pylon_count; ++index) sight_mount_data[aircraft][index] = anchor(static_cast<unsigned int>(static_cast<int>(aircraft_mounts[aircraft]) + (static_cast<int>(index) + first_sight_pylon) * 6));
  for(unsigned int i{0}; i != aircraft_envelopes.size(); ++i) aircraft_envelopes[i] = view.byte(data_file + 0x93b4 + i);
  for(unsigned int i{0}; i != formations.size(); ++i) {
    auto const at{data_file + 0x5984 + i * 14};
    formations[i] = {
      .heading{view.le16(at)},
      .distance{std::bit_cast<int32_t>(view.le32(at + 6))},
      .height{std::bit_cast<int32_t>(view.le32(at + 10))},
      .forward_distance{std::bit_cast<int32_t>(view.le32(at + 2))},
    };
  }
  // Native 0x87114 measures the full byte without the painter's &63 mapping.
  for(unsigned int character{0}; character != measured_advances.size(); ++character) {
    int const index{std::bit_cast<int8_t>(view.byte(mapping + character))};
    // MOVZX AX retains the upper word of the sign-extended index in EAX.
    measured_advances[character] = (index < 0 ? 0xffff0000u : 0u) + view.byte(static_cast<unsigned int>(static_cast<int>(advances) + index)) + 1u;
  }
  for(unsigned int i{0}; i != character_map.size(); ++i) character_map[i] = view.byte(mapping + i);
  for(unsigned int i{0}; i != glyphs.size(); ++i) {
    auto const at{glyph_file + i * 8};
    if(view.byte(at) != 8 || view.byte(at + 1) != 6) throw std::runtime_error{"unexpected native HUD glyph dimensions"};
    glyphs[i].advance = view.byte(advances + i) + 1;
    for(unsigned int row{0}; row != 6; ++row) glyphs[i].rows[row] = view.byte(at + 2 + row);
  }
  for(unsigned int radius{1}; radius != limit; ++radius) {
    auto const spans{view.slice(circle_base + view.le16(circle_base + (radius - 1) * 2), radius)};
    circles[radius].assign(spans.begin(), spans.end());
  }
}

vec3<int16_t> const &executable_tables::sight_mount(flight::player_aircraft const aircraft, int16_t const pylon) const {
  if(pylon < first_sight_pylon || pylon > weapons::cannon_pylon) throw std::out_of_range{"HUD sight requires a native pylon or sentinel"};
  return sight_mount_data.at(std::to_underlying(aircraft) - 1).at(static_cast<unsigned int>(pylon - first_sight_pylon));
}

flight::refuel_assets const &executable_tables::refuel_parameters() const {
  return *refuel_data;
}
presentation::fonts const &executable_tables::presentation_fonts() const {
  return *font_data;
}

presentation::widget_font const &executable_tables::interface_font() const {
  return *widget_font_data;
}
planner_artwork const &executable_tables::planner_art() const {
  return *planner_art_data;
}
presentation::score_assets const &executable_tables::scores() const {
  return *score_data;
}
presentation::arming_assets const &executable_tables::arming_art() const {
  return *arming_data;
}
cockpit::instrument_assets const &executable_tables::instruments() const {
  return *instrument_data;
}
presentation::pilot_assets const &executable_tables::pilots() const {
  return *pilot_assets;
}

std::span<uint8_t const> executable_tables::circle(unsigned int const radius) const {
  /// The native constructor rejects zero and radii at or beyond its table limit
  if(radius >= circles.size()) return {};
  return circles[radius];
}

unsigned int executable_tables::glyph_measure(unsigned char const character) const {
  return character == ' ' ? 3u : measured_advances[character];
}

presentation::configuration_menu_assets const &executable_tables::configuration_widgets() const {
  return *configuration_data;
}

hud_glyph const *executable_tables::glyph(unsigned char const character) const {
  auto const index{character_map[character & 63]};
  return index < glyphs.size() ? &glyphs[index] : nullptr;
}

} // namespace tfx::resources
