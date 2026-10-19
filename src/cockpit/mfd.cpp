#include "mfd.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>
#include "cockpit/format.h"
#include "presentation/drawing.h"
#include "render/sprites.h"

namespace tfx::cockpit {
namespace {

template<typename... Values>
std::string formatted(std::string const &format, Values const... values) {
  std::array<char,128> buffer{};
  int const length{std::snprintf(buffer.data(),buffer.size(),format.c_str(),values...)};
  if(length < 0 || static_cast<unsigned int>(length) >= buffer.size()) throw std::runtime_error{"MFD number exceeds its display buffer"};
  return {buffer.data(),static_cast<unsigned int>(length)};
}

void rectangle(render::framebuffer &target, vec2<int> const &low, vec2<int> const &high, uint8_t const colour, render::framebuffer *const coverage) {
  // Native 0xc98d8; the calling MFD clips the bottom to row 199 before this fill.
  if(low.y >= render::screen_height) return;
  for(int y{std::max(0,low.y)}; y <= std::min(render::screen_height - 1,high.y); ++y) for(int x{std::max(0,low.x)}; x <= std::min(render::screen_width - 1,high.x); ++x) {
    auto const index{static_cast<unsigned int>(y * render::screen_width + x)};
    target.write(index,colour);
    if(coverage) coverage->pixels[index] = 255;
  }
}

} // anonymous namespace

display_page outer_page(display_side const side, unsigned int const selection) {
  // Original 0x88e38/0x8905c use different page tables, including map and engine readouts on the right.
  std::array<display_page,9> constexpr left{
    display_page::stores,display_page::gauges,display_page::flight_data,display_page::fuel,display_page::systems,
    display_page::schematic,display_page::camera,display_page::ground_database,display_page::controls,
  };
  std::array<display_page,9> constexpr right{
    display_page::moving_map,display_page::stores,display_page::flight_data,display_page::fuel,display_page::systems,
    display_page::engine_load,display_page::camera,display_page::gauges,display_page::controls,
  };
  return (side == display_side::left ? left : right).at(selection < left.size() ? selection : 0);
}

mfd::mfd(instrument_assets const &source, resources::archive const &game, std::span<weapons::definition const> const definitions)
  : assets{source}, weapons{definitions}, backgrounds{resources::decode_sprites(game.read(assets.display_bank))}, control_backgrounds{resources::decode_sprites(game.read(assets.controls_bank))} {
}
void mfd::draw(render::software_renderer &renderer, display_side const side, display_page const page, mfd_state const &state, render::framebuffer *const coverage) const {
  auto const &origin{assets.display_origins[side == display_side::left ? 0 : 2]};
  auto const point{[&](vec2<int> const &local) {
    return vec2<int>{math::signed_word(static_cast<uint16_t>(local.x + origin.x + state.offset.x)),math::signed_word(static_cast<uint16_t>(local.y + origin.y + state.offset.y))};
  }};
  auto const background{[&](unsigned int const frame) {
    auto const centre{point({28,18})};
    render::draw_sprite(renderer.output,backgrounds.at(frame),{static_cast<uint16_t>(centre.x),static_cast<uint16_t>(centre.y)},coverage,renderer.clipping);
  }};
  auto const mask{[&](instrument_mask const shape, vec2<int> const &local, uint8_t const colour) {
    auto const position{point(local)};
    render::draw_monochrome(renderer.output,assets.masks[std::to_underlying(shape)],{static_cast<uint16_t>(position.x),static_cast<uint16_t>(position.y)},colour,coverage,renderer.clipping);
  }};
  auto const text{[&](vec2<int> const &local, std::string const &message, uint8_t const colour) {
    auto const position{point(local)};
    renderer.text(position.x,position.y,message,colour,coverage);
  }};
  auto const centred{[&](int const row, std::string const &message, uint8_t const colour) {
    int const width{math::signed_word(static_cast<uint16_t>(renderer.text_width(message)))};
    text({29 - (width >> 1),row},message,colour);
  }};
  auto const status{[&](flight::aircraft_system const system) { return std::to_underlying(state.systems.at(std::to_underlying(system))); }};
  auto const fill{[&](vec2<int> const &low, vec2<int> const &high, uint8_t const colour) {
    rectangle(renderer.output,point(low),point(high),colour,coverage);
  }};
  switch(page) {
  case display_page::stores: {
    if(!state.inventory) throw std::runtime_error{"stores display requires an inventory"};
    background(2);
    auto const &inventory{*state.inventory};
    if(!inventory.guns && (inventory.selected < 0 || static_cast<unsigned int>(inventory.selected) >= inventory.pylons.size() || inventory.pylons[static_cast<unsigned int>(inventory.selected)].weapon >= weapons.size())) throw std::runtime_error{"stores display has no valid selected weapon"};
    unsigned int const aircraft{std::to_underlying(state.aircraft) - 1};
    std::array<unsigned int,3> constexpr visible_mounts{10,4,8}; // Native arming completion 0x89680.
    for(unsigned int index{0}; index != visible_mounts.at(aircraft); ++index) {
      auto const slot{assets.store_order[aircraft][index]};
      auto const &pylon{inventory.pylons[slot]};
      uint8_t colour{pylon.count ? uint8_t{1} : uint8_t{2}};
      if(!inventory.guns && slot == inventory.selected) {
        bool const ready{weapons[pylon.weapon].category == 7 || state.confirmed_target};
        colour = pylon.count ? ready ? uint8_t{11} : uint8_t{14} : uint8_t{2};
      }
      mask(instrument_mask::store,vec2<int>{assets.store_positions[aircraft][index]},colour);
      // The original repeats the selected-store label inside its mount loop.
      if(!inventory.guns) centred(31,weapons[inventory.pylons.at(static_cast<unsigned int>(inventory.selected)).weapon].label,1);
    }
    break;
  }
  case display_page::gauges: {
    background(1);
    int const throttle{std::min(29,static_cast<int>(static_cast<long double>(state.throttle) * assets.gauge_throttle_scale))};
    int const fuel{static_cast<int>(static_cast<long double>(state.fuel) * assets.gauge_fuel_scale)};
    mask(instrument_mask::throttle,{47,30 - throttle},1);
    mask(instrument_mask::fuel,{5,30 - fuel},1);
    break;
  }
  case display_page::flight_data: {
    background(3);
    text({24,2},detail::decimal(assets.flight_formats[0],static_cast<double>(static_cast<long double>(state.speed) * assets.speed_scale),0),3);
    text({24,9},detail::decimal(assets.flight_formats[0],static_cast<double>(-static_cast<long double>(state.height) * assets.altitude_scale),0),1);
    text({24,16},formatted(assets.flight_formats[1],static_cast<unsigned int>(state.heading / 182)),1);
    text({24,23},detail::decimal(assets.flight_formats[0],state.fuel,0),status(flight::aircraft_system::fuel));
    if(state.smoothed_fuel_burn) {
      auto const product{std::bit_cast<int32_t>(static_cast<uint32_t>(state.speed) * static_cast<uint32_t>(static_cast<int32_t>(state.fuel_gauge)))};
      long const range{static_cast<int64_t>(product) / state.smoothed_fuel_burn / 1000};
      text({24,30},formatted(assets.flight_formats[2],range),1);
    } else text({24,30},assets.zero_range,1);
    break;
  }
  case display_page::fuel: {
    background(2);
    int remaining{state.fuel_gauge};
    int constexpr cell_capacity{2500}, cell_steps{8}, gauge_per_pixel{312};
    for(unsigned int index{static_cast<unsigned int>(assets.fuel_cells.size())}; index--;) {
      auto const &cell{assets.fuel_cells[index]};
      vec2<int> const low{cell[0]}, high{cell[1]};
      // The native loop subtracts in a dword but compares only its signed low word.
      int const reading{math::signed_word(static_cast<uint16_t>(remaining))};
      if(reading >= cell_capacity) fill(low,high,1);
      else if(reading <= 0) fill(low,high,10);
      else {
        int const empty_rows{cell_steps - reading / gauge_per_pixel};
        if(empty_rows > 0) {
          fill(low,{high.x,low.y + empty_rows - 1},11);
          if(empty_rows < cell_steps) fill({low.x,low.y + empty_rows},high,1);
        } else fill(low,high,1);
      }
      remaining -= cell_capacity;
    }
    centred(30,detail::decimal(assets.fuel_total_format,state.fuel,0),status(flight::aircraft_system::fuel));
    break;
  }
  case display_page::systems:
    background(4);
    for(unsigned int index{0}; index != state.systems.size(); ++index) {
      if(state.systems[index] != flight::system_condition::normal) text(vec2<int>{assets.system_positions[index]},assets.system_labels[index],status(static_cast<flight::aircraft_system>(index)));
      if(state.systems[index] != flight::system_condition::failed) continue;
      auto const &box{assets.system_boxes[index]};
      auto const low{point(vec2<int>{box[0]})}, high{point(vec2<int>{box[1]})};
      for(auto const &edge : {std::pair{vec2<int>{low},vec2<int>{high.x,low.y}},std::pair{vec2<int>{high.x,low.y},vec2<int>{high}},std::pair{vec2<int>{high},vec2<int>{low.x,high.y}},std::pair{vec2<int>{low.x,high.y},vec2<int>{low}}}) {
        presentation::draw_line(renderer.output,vec2<int>{edge.first},edge.second,11);
        if(coverage) presentation::draw_line(*coverage,vec2<int>{edge.first},edge.second,255);
      }
    }
    break;
  case display_page::engine_load:
    background(2);
    mask(instrument_mask::left_engine,{22,10},status(flight::aircraft_system::left_engine));
    mask(instrument_mask::right_engine,{30,10},status(flight::aircraft_system::right_engine));
    text({2,30},detail::decimal(assets.engine_formats[0],static_cast<double>(static_cast<long double>(state.engine_load[0]) * assets.engine_percent),0),status(flight::aircraft_system::left_engine));
    text({30,30},detail::decimal(assets.engine_formats[1],static_cast<double>(static_cast<long double>(state.engine_load[1]) * assets.engine_percent),0),status(flight::aircraft_system::right_engine));
    break;
  case display_page::schematic:
    background(2);
    for(auto const &position : {vec2<int>{26,13},vec2<int>{19,20},vec2<int>{33,20}}) mask(instrument_mask::gear,position,status(flight::aircraft_system::landing_gear));
    mask(instrument_mask::left_flap,{10,8},status(flight::aircraft_system::flaps));
    mask(instrument_mask::right_flap,{41,8},status(flight::aircraft_system::flaps));
    mask(instrument_mask::lower_flap,{7,27},status(flight::aircraft_system::flaps));
    mask(instrument_mask::lower_flap,{34,27},status(flight::aircraft_system::flaps));
    break;
  case display_page::controls: {
    auto const centre{point({28,18})};
    render::draw_sprite(renderer.output,control_backgrounds.at(0),{static_cast<uint16_t>(centre.x),static_cast<uint16_t>(centre.y)},coverage,renderer.clipping);
    if(state.aircraft != flight::player_aircraft::raptor) {
      fill({2,1},{18,11},5);
      fill({2,13},{18,17},3);
    }
    if(state.aircraft != flight::player_aircraft::nighthawk) {
      auto const pointer{[&](int const x, int const length, int const degrees) {
        // Original truncates to a signed word, wraps the 16.16 numerator, then
        // discards the six angle bits absent from its 1,024-entry sine table.
        int32_t const numerator{std::bit_cast<int32_t>(static_cast<uint32_t>(degrees) * 65536u)};
        auto const angle{static_cast<math::angle>(numerator / 360) & uint16_t{0xffc0}};
        auto const delta{math::rotate_words({0,static_cast<int16_t>(-length)},static_cast<math::angle>(angle))};
        auto const from{point({x,5})};
        vec2<int> const to{math::signed_word(static_cast<uint16_t>(from.x + delta.x)),math::signed_word(static_cast<uint16_t>(from.y + delta.y))};
        presentation::draw_line(renderer.output,vec2<int>{from},to,6);
        if(coverage) presentation::draw_line(*coverage,vec2<int>{from},to,255);
      }};
      auto const deflection{[&](double const value) { return math::signed_word(static_cast<uint16_t>(static_cast<int>(static_cast<long double>(value) * assets.surface_scale))); }};
      pointer(43,10,90 + deflection(state.controls.trailing_edge_flap));
      pointer(32,5,-90 - deflection(state.controls.leading_edge_flap));
      if(state.aircraft == flight::player_aircraft::raptor) {
        double const scale{assets.raptor_degrees / assets.raptor_radians};
        for(unsigned int index{0}; index != 2; ++index) {
          auto const base{point({index ? 12 : 6,6})};
          int const row{static_cast<int>(static_cast<long double>(base.y) + static_cast<long double>(state.controls.thrust_vector[index]) * scale * assets.raptor_pointer_scale)};
          auto const shape{index ? instrument_mask::throttle : instrument_mask::fuel};
          render::draw_monochrome(renderer.output,assets.masks[std::to_underlying(shape)],{static_cast<uint16_t>(base.x),static_cast<uint16_t>(row)},1,coverage,renderer.clipping);
        }
      }
    }
    auto const &autopilot{state.controls.autopilot};
    if(autopilot.enabled) {
      text({27,13},assets.autopilot_label,2);
      auto const mode{std::to_underlying(autopilot.mode)};
      if(mode >= assets.autopilot_mode_labels.size()) break;
      auto const &label{assets.autopilot_mode_labels[mode]};
      long const heading{static_cast<unsigned int>(autopilot.heading) * 360u >> 16};
      if(autopilot.mode == autopilot_mode::waypoint) text({2,20},formatted(label,static_cast<int>(autopilot.waypoint),heading),2);
      else if(autopilot.mode == autopilot_mode::heading) text({2,20},formatted(label,heading),2);
      else text({2,20},label,2);
      double const speed{static_cast<double>(static_cast<long double>(autopilot.speed) * assets.autopilot_speed_scale)};
      if(autopilot.mode == autopilot_mode::waypoint || autopilot.mode == autopilot_mode::heading) {
        double const height{static_cast<double>(-static_cast<long double>(autopilot.height) * assets.autopilot_height_scale)};
        text({2,27},formatted(assets.autopilot_height_speed_format,detail::decimal_value(height,0),detail::decimal_value(speed,0)),2);
      } else if(autopilot.mode != autopilot_mode::recover) text({2,27},detail::decimal(assets.autopilot_speed_format,speed,0),2);
    } else {
      auto const configuration{std::to_underlying(state.controls.configuration)};
      if(configuration < assets.configuration_labels.size()) text({2,20},assets.configuration_labels[configuration],2);
    }
    break;
  }
  default: throw std::runtime_error{"this MFD page still needs its native scene/input integration"};
  }
}

} // namespace tfx::cockpit
