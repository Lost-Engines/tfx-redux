#include "math/floating_environment.h"
#include "simple_turn.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace tfx::flight {
namespace {

using calculation = long double;
float constexpr native_pi{3.141590118408203f};
float constexpr normalised_word{3.0518509447574615e-05f};
float constexpr mach_response{250}, response_fraction{1.6666666269302368f};
float constexpr response_ceiling{150}, bank_response{4.999999873689376e-06f};
double constexpr minimum_turn_mach{.1}, inverse_gravity{.1019367991845056};
float constexpr sound_speed{340}, world_units_per_metre_per_tick{.008196720853447914f};
float constexpr ticks_per_second{50}, feet_per_world_unit{8}, metres_per_foot{.30480000376701355f};
float constexpr load_fraction{.3333333432674408f}, load_scale{3}, load_tail{.20000000298023224f};
int32_t constexpr bank_sector_begin{8192}, bank_sector_midpoint{16384}, bank_sector_end{24576};
int32_t constexpr bank_correction_peak{4096}, bank_correction_divisor{160};
int constexpr maximum_bank_sine{16384};

calculation normalised(int32_t const component) {
  return static_cast<calculation>(math::signed_word(static_cast<uint16_t>(component))) * normalised_word;
}
int32_t truncate(calculation const value) {
  if(!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() || value >= 2147483648.0L) return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}
void rotate_heading(simple_turn_state &state, float const amount) {
  // Native 0x8c008 shares its residual between both heading operations.
  calculation const accumulated{static_cast<calculation>(state.heading_remainder) + amount};
  state.heading_remainder = static_cast<float>(accumulated);
  auto const rotation{static_cast<math::angle>(truncate(accumulated))};
  state.heading_remainder = static_cast<float>(static_cast<calculation>(state.heading_remainder) - math::signed_word(rotation));
  state.orientation.rotate(math::rotation_axis::heading,rotation);
}

} // anonymous namespace

void update_simple_turn(simple_turn_state &state, simple_turn_context const &context) {
  math::floating_environment const environment;
  float const right{static_cast<float>(normalised(state.orientation.right.y))};
  float const right_square{static_cast<float>(normalised(state.orientation.right.y) * right)};
  float const down{static_cast<float>(normalised(state.orientation.down.y))};
  float const bank_weight{static_cast<float>(normalised(state.orientation.down.y) * down + right_square)};
  auto const weighted{truncate(static_cast<calculation>(math::signed_word(state.orientation.angles().z)) * bank_weight)};
  int32_t const bank{math::signed_word(static_cast<uint16_t>(weighted))};
  state.bank_correction = static_cast<int16_t>(state.bank_correction * 3 / 4);
  if(bank < -bank_sector_begin && bank > -bank_sector_end) state.bank_correction = static_cast<int16_t>(std::abs((bank + bank_sector_midpoint) / 2) - bank_correction_peak);
  else if(bank > bank_sector_begin && bank < bank_sector_end) state.bank_correction = static_cast<int16_t>(bank_correction_peak - std::abs((bank - bank_sector_midpoint) / 2));
  int const bank_sine{std::clamp(math::sine_q15(weighted),-maximum_bank_sine,maximum_bank_sine)};
  if(state.bank_correction) rotate_heading(state,static_cast<float>(-state.bank_correction * context.elapsed / bank_correction_divisor));
  state.turn_response = static_cast<float>(static_cast<calculation>(context.mach) * mach_response * response_fraction);
  if(context.mach > minimum_turn_mach) {
    float const limit{static_cast<float>(static_cast<calculation>(response_ceiling) / context.mach)};
    if(state.turn_response > limit) state.turn_response = limit;
  } else state.turn_response = 0;
  float const turn{static_cast<float>(static_cast<calculation>(bank_sine) * bank_response * state.turn_response)};
  state.orientation = state.orientation.transposed();
  rotate_heading(state,static_cast<float>(static_cast<calculation>(context.elapsed) * turn));
  state.orientation = state.orientation.transposed();
  calculation const speed{static_cast<calculation>(context.mach) * sound_speed};
  state.movement_per_tick = static_cast<float>(speed * world_units_per_metre_per_tick);
  state.coordinated_turn_rate = static_cast<float>(static_cast<calculation>(turn) * ticks_per_second * native_pi * normalised_word);
  float const pitch{static_cast<float>(static_cast<calculation>(native_pi) * context.pitch_rate * normalised_word * ticks_per_second)};
  state.metres_per_second = static_cast<float>(static_cast<calculation>(state.movement_per_tick) * ticks_per_second * feet_per_world_unit * metres_per_foot);
  float const turning_speed{static_cast<float>(speed * state.coordinated_turn_rate)};
  double const coupling{static_cast<double>(-static_cast<calculation>(pitch) * state.metres_per_second * inverse_gravity - normalised(state.orientation.right.y) * turning_speed * inverse_gravity)};
  state.g_load = static_cast<float>((normalised(state.orientation.down.y) * 5.0f * load_fraction + coupling) * load_scale * load_tail);
}

} // namespace tfx::flight
