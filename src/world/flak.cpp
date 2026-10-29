#include "flak.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace tfx::world {
namespace {
int32_t add(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t subtract(int32_t const a, int32_t const b) { return std::bit_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t absolute(int32_t const value) { return value < 0 ? subtract(0,value) : value; }
}

std::optional<flak_event> advance_flak(flak_state &state, flak_context const &context) {
  // Native 0x7cc30 advances once per game frame, not once per elapsed tick.
  int32_t constexpr highest_flak_y{-1500}, lowest_flak_y{-150};
  if(context.regions.empty() || context.player.y < highest_flak_y || context.player.y > lowest_flak_y || (context.cloud_layer && context.layer_y > context.player.y)) return std::nullopt;
  if(state.delay) state.delay = math::signed_word(static_cast<uint16_t>(state.delay - 1));
  if(state.delay) return std::nullopt;
  auto const region{std::ranges::find_if(context.regions,[&](missions::flak_region const &area) {
    return area.first.x <= context.player.x && area.last.x >= context.player.x && area.first.y <= context.player.z && area.last.y >= context.player.z;
  })};
  if(region == context.regions.end()) return std::nullopt;
  state.centre = {context.player.x,context.player.z};
  uint16_t const phase{state.random.phase};
  state.delay = phase & 1;
  // This dedicated generator uses signed SAR/word wrapping; it is not the
  // game's ADD/ADC generator and does not consume the combat random stream.
  state.random.phase = static_cast<uint16_t>(phase + state.random.scatter);
  state.random.scatter = static_cast<uint16_t>(state.random.scatter + (math::signed_word(phase) >> 1) + (static_cast<uint32_t>(phase) << 15));
  if(!context.night) state.random.scatter = static_cast<uint16_t>(state.random.scatter + 50);
  unsigned int constexpr day_radius_mask{1023}, night_radius_mask{2047};
  int32_t constexpr flash_radius{80};
  int32_t const radius{static_cast<int32_t>(state.random.scatter & (context.night ? night_radius_mask : day_radius_mask))};
  int32_t const x{-radius * math::sine_q15(phase) >> math::matrix_fraction_bits};
  int32_t const z{radius * math::cosine_q15(phase) >> math::matrix_fraction_bits};
  bool const flash{!context.night && radius < flash_radius};
  uint8_t const model{context.night ? state.night_model : uint8_t{0}};
  if(context.night && ++state.night_model >= flak_night_models) state.night_model = 0;
  return flak_event{
    .position{add(context.player.x,x),context.night ? 0 : add(context.player.y,static_cast<int32_t>(phase & 63) - 32),add(context.player.z,z)},
    .model{model},
    .sample{flash && (state.random.scatter & 1) != 0},
    .radio{flash && context.radio_idle},
    .flash{flash},
  };
}

bool flak_visible(flak_state const &state, vec3<int32_t> const &player) {
  // Native 0x7cf34 keeps its signed absolute-value quirk at INT32_MIN.
  int32_t constexpr maximum_separation{32767};
  return std::max(absolute(subtract(state.centre.x,player.x)),absolute(subtract(state.centre.y,player.z))) <= maximum_separation;
}
void flak_pool::reset() { next = flak_capacity - 1;count = 0; }
unsigned int flak_pool::spawn(flak_event const &event) {
  if(event.model >= flak_night_models) throw std::out_of_range{"unknown night flak model"};
  unsigned int const slot{next};
  records[slot] = {
    .position{math::signed_word(static_cast<uint16_t>(event.position.x)),math::signed_word(static_cast<uint16_t>(event.position.y)),math::signed_word(static_cast<uint16_t>(event.position.z))},
    .model{event.model},
  };
  next = next ? next - 1 : flak_capacity - 1;
  if(count != flak_capacity) ++count;
  return slot;
}
flak_record const &flak_pool::record(unsigned int const slot) const { return records.at(slot); }

} // namespace tfx::world
