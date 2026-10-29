#include "palette_lighting.h"
#include <bit>

namespace tfx::render {
namespace {

int16_t view_phase(math::angle const pitch, math::angle const roll) {
  auto const cosine{math::cosine_q15};
  int constexpr phase_radius{lighting_stages / 2};
  int const vertical{cosine(pitch) * phase_radius >> math::matrix_fraction_bits};
  int const banked{cosine(roll) * phase_radius >> math::matrix_fraction_bits};
  return static_cast<int16_t>(vertical * banked / phase_radius + phase_radius);
}

} // anonymous namespace

palette_lighting::palette_lighting(std::array<dac_palette,altitude_palette_count> const &source_banks, std::array<int32_t,altitude_palette_count> const &source_altitudes, dac_palette const &source_fixed, palette_lighting_state const initial) :
  banks{source_banks}, altitudes{source_altitudes}, fixed{source_fixed}, current{source_banks[0]}, state{initial} {
  // Native startup forces the base palette's first RGB triple to black.
  for(unsigned int channel{0}; channel != 3; ++channel) banks[0][channel] = current[channel] = 0;
}
palette_lighting_state const &palette_lighting::value() const { return state; }
dac_palette const &palette_lighting::colours() const { return current; }
void palette_lighting::flash(uint8_t const level) { state.flash = level; }
void palette_lighting::blend(dac_palette const &from, dac_palette const &to, int16_t const level) {
  // Native 0x72ccc skips the magenta source sentinel, leaving existing bytes.
  for(unsigned int index{0}; index != 256; ++index) {
    auto const offset{index * 3};
    if(from[offset] == 63 && from[offset + 1] == 0 && from[offset + 2] == 63) continue;
    for(unsigned int channel{0}; channel != 3; ++channel) {
      int const base{std::bit_cast<int8_t>(from[offset + channel])};
      int const target{std::bit_cast<int8_t>(to[offset + channel])};
      current[offset + channel] = static_cast<uint8_t>((base + (target - base) * level / static_cast<int>(lighting_stages)) & 63);
    }
  }
}
void palette_lighting::tint(vec3<uint8_t> const &colour, int16_t const level) {
  // Native 0x72dc0 includes reserved entries and truncates towards zero.
  for(unsigned int index{0}; index != current.size(); ++index) {
    int const base{colour[index % 3]};
    int const target{std::bit_cast<int8_t>(current[index])};
    current[index] = static_cast<uint8_t>((base + (target - base) * level / static_cast<int>(lighting_stages)) & 63);
  }
}
palette_lighting_result palette_lighting::advance(palette_lighting_context const &context) {
  palette_lighting_result result{
    .changed{context.normal_changed},
  };
  int16_t const phase{view_phase(context.pitch,context.roll)};
  // Native 0x73214 leaves the retained altitude palette alone in target view.
  if(context.view != cockpit::view_mode::target) {
    int16_t constexpr refresh_step{1000};
    if(phase != state.view_phase || context.blackout != blackout_mode::none || state.flash) state.altitude_step = refresh_step;
    int32_t height{std::bit_cast<int32_t>(0u - static_cast<uint32_t>(context.camera_y))};
    unsigned int band{0};
    while(band != altitude_palette_count - 1 && height >= altitudes[band]) ++band;
    int32_t step{height};
    if(band) {
      if(band == altitude_palette_count - 1 && height > altitudes.back()) height = altitudes.back() - 1;
      int32_t const span{(altitudes[band] - altitudes[band - 1]) / static_cast<int>(lighting_stages)};
      step = (height - altitudes[band - 1]) / (span ? span : 1);
    }
    if(step != state.altitude_step) {
      unsigned int const next{band ? band : 1};
      auto const level{math::signed_word(static_cast<uint16_t>(lighting_stages - (band ? step : 0)))};
      blend(banks[next],banks[band ? band - 1 : 0],level);
      state.altitude_step = math::signed_word(static_cast<uint16_t>(step));
      result.changed = true;
    }
  }
  // Native 0x73f1b keeps the adjustable HUD green component independent.
  current[6 * 3 + 1] = context.hud_green;
  if(phase != state.view_phase || result.changed) {
    blend(fixed,current,phase);
    state.view_phase = phase;
    result.changed = true;
    if(context.effects) {
      if(state.flash) {
        tint({63,63,63},state.flash);
        if(++state.flash > lighting_stages) state.flash = 0;
      }
      if(context.blackout == blackout_mode::redout) {
        result.sound = context.redout_level < 10 ? 65 : context.redout_level < 25 ? 66 : 67;
        tint({63,0,0},context.redout_level);
      } else if(context.blackout == blackout_mode::blackout) {
        // Original 0x73b42 tests the redout word for its second threshold.
        result.sound = context.blackout_level < 10 ? 65 : context.redout_level < 25 ? 66 : 67;
        tint({0,0,0},context.blackout_level);
      }
    }
  }
  return result;
}

} // namespace tfx::render
