#include "weapon_display.h"
#include <utility>
#include "cockpit/landing_display.h"
#include "presentation/drawing.h"
#include "render/sprites.h"
#include "weapons/definition.h"

namespace tfx::cockpit {
namespace {

int32_t constexpr short_sight_range_limit{7004}, compact_sight_range{24720};
uint8_t constexpr lock_blink_frames{9}, launch_message_frames{5};

} // anonymous namespace

weapon_display::weapon_display(instrument_assets const &source, resources::archive const &game)
  : assets{source},
    attitude{source, game},
    numbers{source},
    targets{source},
    alerts{source},
    framing{resources::decode_sprites(game.read(assets.aiming_bank)).at(0)} {
}
void weapon_display::reset_tracking() {
  tracking = {};
}
void weapon_display::show_launch() {
  tracking.launch_frames = launch_message_frames;
}
target_animation weapon_display::animation() const {
  return tracking;
}
void weapon_display::check_fuel(double const fuel) {
  alerts.check_fuel(fuel);
}
void weapon_display::show_alert(hud_alert const alert) {
  alerts.show(alert);
}
hud_alert_state weapon_display::alert_state() const {
  return alerts.value();
}
weapon_effects weapon_display::draw_forward(render::software_renderer &renderer, weapon_display_state const &state, render::framebuffer *const coverage) {
  // Native 0x800f4 ordering; general navigation/radar cues remain separate.
  auto const &offset{state.attitude.offset};
  auto const full_clip{[&]{renderer.clipping = render::raster_clip::viewport({0, 0}, {319, 199});}};
  full_clip();
  if(state.navigation_target) {
    auto const &cue{*state.navigation_target};
    target_display_state const contact{
      .player{vec3<int32_t>{state.flight.position}},
      .orientation{cue.orientation},
      .projected{vec2<int16_t>{cue.projected}},
      .tracking_q8{vec2<int32_t>{cue.tracking_q8}},
      .sight{tracking.sight},
      .offset{vec2<int16_t>{offset}},
    };
    // Native 0x80010 precedes acquisition and the weapon layout. Its optional
    // alert draw consumes another elapsed word before the later layout draw.
    targets.direction(renderer, cue.reading, contact, {0, 0}, {319, 199}, coverage);
    targets.information(renderer, cue.reading, contact, coverage);
    alerts.draw(renderer, offset, state.elapsed, coverage);
    if(cue.projected.x > 0 && cue.projected.y > 0) {
      renderer.clipping = state.limited ? render::raster_clip::viewport({80 + offset.x, 2 + offset.y}, {241 + offset.x, 144 + offset.y}) : render::raster_clip::viewport({0, 0}, {319, 199});
      targets.marker(renderer, tracking.sight, false, vec2<int>{cue.projected}, coverage);
    }
  }
  if(state.limited && !state.cannon && (state.mode != aiming_mode::navigation || !state.navigation_target)) {
    render::draw_monochrome(renderer.output, assets.masks[std::to_underlying(instrument_mask::weapon_centre_cue)], {static_cast<uint16_t>(150 + offset.x), state.attitude.projection_origin.y}, 6, coverage, renderer.clipping);
  }
  if(state.aerodynamic) numbers.draw_aerodynamic(renderer, *state.aerodynamic, offset, coverage);
  auto const line_limit{attitude.draw_heading(renderer, state.attitude, coverage)};
  full_clip();
  numbers.draw_values(renderer, state.flight, coverage);
  if(state.mode != aiming_mode::navigation) {
    numbers.draw_messages(renderer, state.flight, coverage);
    renderer.clipping = render::raster_clip::viewport({91 + offset.x, 4 + offset.y}, {231 + offset.x, 97 + offset.y});
    render::draw_sprite(renderer.output, framing, {static_cast<uint16_t>(162 + offset.x), static_cast<uint16_t>(56 + offset.y)}, coverage, renderer.clipping);
    if(state.cannon) {
      vec2<int> const low{89 + offset.x, 2 + offset.y};
      vec2<int> const high{231 + offset.x, 96 + offset.y};
      for(unsigned int index{0}; index != 4; ++index) {
        auto const &from{state.cannon_path[index]};
        auto const &to{state.cannon_path[index + 1]};
        presentation::draw_clipped_line(renderer.output, vec2<int>{from}, vec2<int>{to}, low, high, 6);
        if(coverage) presentation::draw_clipped_line(*coverage, vec2<int>{from}, vec2<int>{to}, low, high, 255);
      }
    }
  }
  vec2<int> const weapon_line_limit{state.cannon ? 231 + offset.x : line_limit.x, state.cannon ? 96 + offset.y : line_limit.y};
  draw(renderer, state, coverage, weapon_line_limit);
  bool const new_lock{state.mode != aiming_mode::navigation && !tracking.had_lock && state.air_lock};
  if(new_lock) tracking.blink = lock_blink_frames;
  if(state.mode == aiming_mode::air && state.limited) tracking.sight = state.range >= compact_sight_range ? target_sight::compact : target_sight::normal;
  if(state.mode != aiming_mode::navigation) {
    renderer.clipping = state.limited ? render::raster_clip::viewport({80 + offset.x, 2 + offset.y}, {241 + offset.x, 144 + offset.y}) : render::raster_clip::viewport({0, 0}, {319, 199});
    if(state.mode == aiming_mode::air && !state.suppressed && state.air_target) {
      auto const &cue{*state.air_target};
      target_display_state const target_state{
        .player{vec3<int32_t>{state.flight.position}},
        .orientation{cue.orientation},
        .projected{vec2<int16_t>{cue.projected}},
        .tracking_q8{vec2<int32_t>{cue.tracking_q8}},
        .sight{tracking.sight},
        .offset{vec2<int16_t>{offset}},
      };
      targets.information(renderer, cue.reading, target_state, coverage);
      if(state.air_lock) {
        if(tracking.blink) --tracking.blink;
        vec2<int> const centre{cue.tracking_q8.x >> 8, cue.tracking_q8.y >> 8};
        if(centre.x > 0 && centre.y > 0) targets.marker(renderer, tracking.sight, false, centre, coverage);
        if(!(tracking.blink & 1) && cue.tracking_q8.x && cue.tracking_q8.y) targets.marker(renderer, tracking.sight, true, centre, coverage);
      } else {
        if(cue.projected.x > 0 && cue.projected.y > 0) targets.marker(renderer, tracking.sight, false, vec2<int>{cue.projected}, coverage);
        if(cue.acquiring) {
          targets.edge_marker(renderer, target_state, coverage);
        } else {
          targets.direction(renderer, cue.reading, target_state, state.limited ? vec2<int>{0, 2 + offset.y} : vec2<int>{89 + offset.x, 2 + offset.y}, state.limited ? vec2<int>{319, 144 + offset.y} : vec2<int>{231 + offset.x, 96 + offset.y}, coverage);
        }
      }
    } else if(state.mode == aiming_mode::ground && state.ground_lock && state.ground_marker && state.category != weapons::cruise_missile_category) {
      // Native ground-designation rectangle keeps fixed X limits in both views.
      auto const &centre{*state.ground_marker};
      vec2<int> const low{state.limited ? 89 : 10, state.limited ? 2 + offset.y : 10};
      vec2<int> const high{state.limited ? 233 : 310, state.limited ? 97 + offset.y : 190};
      std::array<vec2<int>, 4> const corners{{{-10, -10}, {10, -10}, {10, 10}, {-10, 10}}};
      for(unsigned int index{0}; index != corners.size(); ++index) {
        auto const from{vec2<int>{centre} + corners[index]};
        auto const to{vec2<int>{centre} + corners[(index + 1) % corners.size()]};
        presentation::draw_clipped_line(renderer.output, vec2<int>{from}, vec2<int>{to}, low, high, 6);
        if(coverage) presentation::draw_clipped_line(*coverage, vec2<int>{from}, vec2<int>{to}, low, high, 255);
      }
    }
    tracking.had_lock = state.air_lock;
  }

  if(state.mode == aiming_mode::navigation) {
    // Native NAV draws its alert before the outer brake/chute message repeat.
    alerts.draw(renderer, offset, state.elapsed, coverage);
    full_clip();
    numbers.draw_messages(renderer, state.flight, coverage);
  } else if(state.bomb_impact) {
    // Native 0x7f798: the impact marker is conditional, but its connecting
    // line is always sent to the original bounded weapon viewport.
    auto const &impact{*state.bomb_impact};
    if(impact.x >= 116 + offset.x && impact.x <= 193 + offset.x && impact.y >= 18 + offset.y && impact.y <= 94 + offset.y) render::draw_monochrome(renderer.output, assets.masks[std::to_underlying(instrument_mask::impact_sight)], {impact.x, impact.y}, 6, coverage, renderer.clipping);
    vec2<int> const origin{160 + offset.x, state.attitude.projection_origin.y};
    vec2<int> const low{89 + offset.x, 2 + offset.y};
    vec2<int> const high{231 + offset.x, 96 + offset.y};
    presentation::draw_clipped_line(renderer.output, vec2<int>{origin}, vec2<int>{impact}, low, high, 6);
    if(coverage) presentation::draw_clipped_line(*coverage, vec2<int>{origin}, vec2<int>{impact}, low, high, 255);
  }
  if(state.mode != aiming_mode::navigation) alerts.draw(renderer, offset, state.elapsed, coverage);
  return {.lock_acquired{new_lock}};
}
void weapon_display::draw(render::software_renderer &renderer, weapon_display_state const &state, render::framebuffer *const coverage, vec2<int> const &line_limit) {
  uint8_t constexpr colour{6};
  auto const &offset{state.attitude.offset};
  auto const point{[&](vec2<int> const &local){
    return vec2<int>{math::signed_word(static_cast<uint16_t>(local.x + offset.x)), math::signed_word(static_cast<uint16_t>(local.y + offset.y))};
  }};
  auto const clip{[&](vec2<int> const &minimum, vec2<int> const &maximum){
    renderer.clipping = render::raster_clip::viewport(point(minimum), point(maximum));
  }};
  auto const full_clip{[&]{renderer.clipping = render::raster_clip::viewport({0, 0}, {319, 199});}};
  auto const text{[&](vec2<int> const &local, std::string_view const message){
    auto const position{point(local)};
    renderer.text(position.x, position.y, message, colour, coverage);
  }};
  auto const right{[&](int const edge, std::string_view const message){text({edge - static_cast<int>(renderer.text_width(message)), 92}, message);}};
  auto const mask{[&](instrument_mask const shape, vec2<int> const &local){
    auto const position{point(local)};
    render::draw_monochrome(renderer.output, assets.masks[std::to_underlying(shape)], {static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y)}, colour, coverage, renderer.clipping);
  }};
  auto const frame{[&]{
    auto const centre{point({162, 56})};
    render::draw_sprite(renderer.output, framing, {static_cast<uint16_t>(centre.x), static_cast<uint16_t>(centre.y)}, coverage, renderer.clipping);
  }};
  auto const line{[&](vec2<int> const &from, vec2<int> const &to, vec2<int> const &minimum, vec2<int> const &maximum){
    presentation::draw_clipped_line(renderer.output, vec2<int>{from}, vec2<int>{to}, minimum, maximum, colour);
    if(coverage) presentation::draw_clipped_line(*coverage, vec2<int>{from}, vec2<int>{to}, minimum, maximum, 255);
  }};
  auto const cannon_label{[&](unsigned int const prefix){text({96, 91}, assets.aiming_labels[prefix] + std::to_string(state.ammunition));}};
  auto const pitch{[&]{
    if(state.limited) attitude.draw_pitch(renderer, state.attitude, coverage, state.mode == aiming_mode::navigation ? vec2<int>{319, 199} : vec2<int>{line_limit});
  }};
  if(state.mode == aiming_mode::navigation) {
    // Native 0x7ecb4 replaces ammunition/NAV labels with active ILS guidance.
    full_clip();
    pitch();
    full_clip();
    numbers.draw_messages(renderer, state.flight, coverage);
    if(!state.landing && state.aircraft != flight::player_aircraft::nighthawk) cannon_label(0);
    if(!state.landing) right(225, assets.aiming_labels[2]);
    frame();
    if(state.landing) draw_landing_cue(renderer, assets, *state.landing, offset, coverage);
    auto const low{point({89, 2})};
    auto const high{point({231, 96})};
    for(unsigned int index{0}; index != 4; ++index) line(vec2<int>{state.cannon_path[index]}, vec2<int>{state.cannon_path[index + 1]}, low, high);
    return;
  }
  if(state.mode == aiming_mode::air) {
    // Native 0x7e5b4: range-dependent sight, suppression cross and AA legend.
    if(state.limited) {
      if(state.range < short_sight_range_limit) {
        mask(instrument_mask::short_range_sight, {122, 18});
      } else if(state.range < compact_sight_range) {
        mask(instrument_mask::medium_range_sight, {122, 18});
      } else {
        mask(instrument_mask::long_range_sight, {132, 34});
      }
    }
    if(state.suppressed) {
      // These original C4CB6 calls use the current world viewport directly.
      auto const low{point({0, 2})};
      auto const high{point({319, 144})};
      line(point({127, 23}), point({192, 88}), low, high);
      line(point({193, 23}), point({126, 90}), low, high);
    }
    if(state.cannon) {
      cannon_label(0);
    } else {
      text({96, 91}, state.weapon);
    }
    right(230, assets.aiming_labels[3]);
    pitch();
    clip({89, 2}, {231, 96});
    if(tracking.launch_frames) {
      text({96, 85}, assets.launch_label);
      --tracking.launch_frames;                                                // Native 0x7e8fa counts AA paints, independently of elapsed ticks.
    } else {
      text({96, 85}, assets.aiming_labels[state.air_lock ? 5 : 6]);
    }
    if(state.air_target) targets.heading(renderer, state.air_target->reading.heading, state.attitude.angles.y, offset, point({89, 2}), point({231, 96}), coverage);
    return;
  }
  if(state.mode != aiming_mode::ground) return;                                // Tank/other roles retain the common framing only.
  // Native 0x7e958: the ground sight differs for strafing and carried stores.
  if(state.ground_target) targets.ground(renderer, state.ground_target->position, state.ground_target->display, {0, 0}, line_limit, coverage);
  if(state.cannon) {
    cannon_label(1);
    vec2<int> const position{point({150, 0}).x, math::signed_word(static_cast<uint16_t>(state.attitude.projection_origin.y - 8))};
    render::draw_monochrome(renderer.output, assets.masks[std::to_underlying(instrument_mask::strafe_sight)], {static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y)}, colour, coverage, renderer.clipping);
  } else {
    mask(instrument_mask::ground_sight, {132, 28});
    text({96, 91}, state.weapon);
  }
  right(230, assets.aiming_labels[4]);
  pitch();
  full_clip();
  text({96, 85}, assets.aiming_labels[state.ground_lock ? 5 : 6]);
}

} // namespace tfx::cockpit
