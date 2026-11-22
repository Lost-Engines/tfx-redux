#include "radar.h"
#include <cstdio>
#include <optional>
#include <stdexcept>
#include "presentation/drawing.h"
#include "render/markers.h"
#include "render/sprites.h"

namespace tfx::cockpit {

radar::radar(instrument_assets const &source, resources::archive const &game)
  : assets{source},
    backgrounds{resources::decode_sprites(game.read(assets.radar_bank))} {
}
bool radar::phase() const {
  return pulse;
}

void radar::draw(render::software_renderer &renderer, radar_mode const mode, unsigned int const range, radar_state const &state, math::random_state &random, render::framebuffer *const coverage) {
  bool const horizontal{mode == radar_mode::horizontal};
  auto const &scale{horizontal ? assets.horizontal_scales.at(range) : assets.bvr_scales.at(range)};
  if(scale.world_per_pixel <= 0) throw std::runtime_error{"invalid native radar scale"};
  renderer.clipping = render::raster_clip::viewport({0, 0}, {render::screen_width - 1, render::screen_height - 1});
  render::draw_sprite(renderer.output, backgrounds.at(horizontal ? 3u : 4u), {static_cast<uint16_t>((horizontal ? 162 : 161) + state.offset.x), static_cast<uint16_t>(173 + state.offset.y)}, coverage, renderer.clipping);
  auto const point{[&](vec2<int> const &position, uint8_t const colour, render::marker_kind const kind = render::marker_kind::pixel){
    render::draw_marker(renderer.output, {static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y)}, colour, kind, coverage);
  }};
  auto const project{[&](vec3<int32_t> const &position){
    auto const delta{[](int32_t const to, int32_t const from){return std::bit_cast<int32_t>(static_cast<uint32_t>(to) - static_cast<uint32_t>(from));}};
    vec2<int16_t> const relative{
      math::signed_word(static_cast<uint16_t>(delta(position.x >> 8, state.player.x) / scale.world_per_pixel)),
      math::signed_word(static_cast<uint16_t>(delta(state.player.z, position.z >> 8) / scale.world_per_pixel)),
    };
    return math::rotate_words(relative, static_cast<math::angle>(-state.heading));
  }};
  auto const passes_fault_filter{[&]{return state.condition != flight::system_condition::intermittent || (random.next() & 1);}};
  if(horizontal) {
    // Original 0x82a2c changes this phase even when stealth/failure immediately suppresses contacts.
    pulse = !pulse;
    auto const self_row{static_cast<uint16_t>(172 + state.offset.y)};
    if(self_row < render::screen_height) point({160 + state.offset.x, self_row}, 1);
    if((state.stealth && state.aircraft != flight::player_aircraft::nighthawk) || state.condition == flight::system_condition::failed) return;
    auto const position{[&](vec3<int32_t> const &world){
      auto const local{project(world)};
      return vec2<int>{math::signed_word(static_cast<uint16_t>(local.x + 160 + state.offset.x)), math::signed_word(static_cast<uint16_t>(local.y + 172 + state.offset.y))};
    }};
    auto const inside{[&](vec2<int> const &pixel, bool const projectile = false){
      return pixel.x > 132 + state.offset.x && pixel.x < 189 + state.offset.x && pixel.y > (projectile ? 151 : 150) + state.offset.y && pixel.y < (projectile ? 193 : 194) + state.offset.y && pixel.y < render::screen_height;
    }};
    int16_t constexpr radiating_defence_type{56};                              // SSD defop1/2/3; also the anti-radiation target class.
    bool const both{state.guns || state.aircraft == flight::player_aircraft::nighthawk};
    if(both || state.selected_role == weapons::weapon_role::air) {
      for(auto const &contact : state.allied_aircraft) {
        if((contact.position_q8.y >> 8) >= -38) continue;
        auto const pixel{position(contact.position_q8)};
        if(inside(pixel) && passes_fault_filter()) point(pixel, 6);
      }
      for(auto const &contact : state.enemy_aircraft) {
        if((contact.position_q8.y >> 8) >= -38) continue;
        auto const pixel{position(contact.position_q8)};
        if(inside(pixel) && passes_fault_filter()) point(pixel, 11, contact.selected && pixel.y < render::screen_height - 1 ? render::marker_kind::block : render::marker_kind::pixel);
      }
    }
    if(both || state.selected_role == weapons::weapon_role::ground) {
      // Allied surface contacts bypass the intermittent-radar sample in this view.
      for(auto const &contact : state.allied_surface) {
        auto const pixel{position(contact.position_q8)};
        if(inside(pixel)) point(pixel, 7);
      }
      for(auto const &contact : state.enemy_surface) {
        auto const pixel{position(contact.position_q8)};
        if(inside(pixel) && (contact.type != radiating_defence_type || pulse) && passes_fault_filter()) point(pixel, 12);
      }
    }
    for(auto const &projectile : state.projectiles) {
      if(projectile.weapon == weapons::weapon_id::cannon || projectile.weapon == weapons::weapon_id::chaff || projectile.weapon == weapons::weapon_id::flare) continue;
      auto const pixel{position(projectile.position_q8)};
      if(!inside(pixel, true)) continue;
      uint8_t const colour{projectile.player_owned ? uint8_t{14} : uint8_t{11}};
      point(pixel + vec2<int>{-1, 0}, colour);
      point(pixel + vec2<int>{1, 0}, colour);
      point(pixel + vec2<int>{0, -1}, colour);
      point(pixel + vec2<int>{0, 1}, colour);
      if(pulse && passes_fault_filter()) point(pixel, colour);
    }
  } else {
    // Original 0x83198 uses the authored per-row envelope, without the horizontal view's role/failure gates.
    int const row{151 + state.offset.y};
    if(row < render::screen_height) {
      vec2<int> const from{133 + state.offset.x, row};
      vec2<int> const to{188 + state.offset.x, row};
      presentation::draw_line(renderer.output, vec2<int>{from}, to, 5);
      if(coverage) presentation::draw_line(*coverage, vec2<int>{from}, to, 255);
    }
    if(192 + state.offset.y < render::screen_height) point({160 + state.offset.x, 192 + state.offset.y}, 1);
    if(state.stealth) return;
    auto const position{[&](vec3<int32_t> const &world)->std::optional<vec2<int>> {
      auto const local{project(world)};
      int const x{math::signed_word(static_cast<uint16_t>(local.x + 160))};
      int const y{math::signed_word(static_cast<uint16_t>(local.y + 192))};
      if(y <= 152 || y >= 193) return {};
      auto const &bounds{assets.bvr_envelope.at(static_cast<unsigned int>(math::signed_word(static_cast<uint16_t>(local.y + 39))))};
      if(x < bounds.x || x > bounds.y) return {};
      vec2<int> const pixel{math::signed_word(static_cast<uint16_t>(x + state.offset.x)), math::signed_word(static_cast<uint16_t>(y + state.offset.y))};
      if(pixel.y >= render::screen_height) return {};
      return vec2<int>{pixel};
    }};
    auto const aircraft{[&](std::span<radar_aircraft const> const contacts, uint8_t const colour){
      for(auto const &contact : contacts) {
        if((contact.position_q8.y >> 8) >= -38) continue;
        if(auto const pixel{position(contact.position_q8)}; pixel && passes_fault_filter()) point(*pixel, colour);
      }
    }};
    auto const surface{[&](std::span<radar_surface const> const contacts, uint8_t const colour){
      for(auto const &contact : contacts) {
        if(auto const pixel{position(contact.position_q8)}; pixel && passes_fault_filter()) point(*pixel, colour);
      }
    }};
    aircraft(state.enemy_aircraft, 11);
    aircraft(state.allied_aircraft, 6);
    surface(state.allied_surface, 7);
    surface(state.enemy_surface, 12);
  }
  std::array<char, 128> buffer{};
  int const length{std::snprintf(buffer.data(), buffer.size(), assets.radar_format.c_str(), static_cast<int>(scale.label))};
  if(length < 0 || static_cast<unsigned int>(length) >= buffer.size()) throw std::runtime_error{"radar range label exceeds its buffer"};
  renderer.text((horizontal ? 178 : 177) + state.offset.x, 188 + state.offset.y, {buffer.data(), static_cast<unsigned int>(length)}, 1, coverage);
}

} // namespace tfx::cockpit
