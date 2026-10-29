#include "air_motion.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace tfx::actors {
namespace {

int constexpr maximum_turn{182}, altitude_offset{105600}, altitude_interval{211200}, minimum_envelope_speed{113}, maximum_envelope_speed{1462}, envelope_speed_interval{75};
int32_t wrap(int64_t const value) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value));
}
int16_t word(int const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}
bool small_angle(int16_t const value, int const threshold) {
  // Native 0x7524c negates before sign-extending AX, so -32768 compares as a large unsigned magnitude.
  return value != -32768 && std::abs(static_cast<int>(value)) < threshold;
}
void reduce_ticks(air_state &state, int const ticks) {
  if(ticks > 0 && ticks < state.input_ticks) state.input_ticks = static_cast<int16_t>(ticks);
}
std::optional<unsigned int> altitude_row(air_state const &state, air_performance const &performance) {
  int32_t const altitude{wrap(-static_cast<int64_t>(state.pose.position.y))};
  if(performance.envelope < 1 || performance.envelope > envelope_classes || altitude < -altitude_offset || altitude >= altitude_offset + altitude_interval * 9) return {};
  return static_cast<unsigned int>((altitude + altitude_offset) / altitude_interval);
}
uint8_t envelope_value(std::span<uint8_t const> const table, air_performance const &performance, unsigned int const row, unsigned int const column) {
  if(table.size() != envelope_bytes) throw std::runtime_error{"missing native aircraft turn envelopes"};
  return table[(performance.envelope - 1) * envelope_altitudes * envelope_columns + row * envelope_columns + column];
}
int16_t bank_component(int16_t const rate, int16_t const sine) {
  // Native 0x77f34 adds the rate (not the sine) before doubling the wrapped product.
  auto const product{wrap((static_cast<int64_t>(rate) * sine + rate) * 2)};
  return word(product >> 16);
}

} // anonymous namespace

air_performance decode_air_performance(profile const &source) {
  // Missions 721/923 dispatch a surface DESTROYER through the aircraft controller; preserve its native cross-record reads within an owned, bounded window.
  resources::binary_view const view{source.native_window.empty() ? source.source : source.native_window};
  return {
    .acceleration{std::bit_cast<int32_t>(view.le32(36))},
    .deceleration{std::bit_cast<int32_t>(view.le32(40))},
    .envelope{view.le16(44)},
    .turn_dividend{view.le16(46)},
    .minimum_speed{view.le16(48)},
    .maximum_speed{view.le16(50)},
    .minimum_altitude{view.le16(52)},
    .maximum_altitude{view.le16(54)},
    .afterburner_speed{view.le16(56)},
    .maximum_roll_rate{view.signed_le16(62)},
  };
}

void reset_air_controls(air_state &state) {
  state.angular_rate = vec3<int16_t>{0,0,0};
  state.acceleration = 0;
  state.input_ticks = air_control_horizon;
  state.pose.flags &= static_cast<uint16_t>(~std::to_underlying(air_flag::waypoint_steering));
}

uint16_t maximum_turn_rate(air_state const &state, air_performance const &performance, air_control_context const &context) {
  /// Native 0x75dfc / 0xa4b54: Arcade divides the profile's turn dividend by speed; other models can select a sampled envelope.
  int const speed{math::signed_word(static_cast<uint16_t>(state.pose.speed >> speed_fraction_bits))};
  if(context.model != simulation::flight_model::arcade && performance.envelope >= 1 && performance.envelope <= envelope_classes) {
    auto const row{altitude_row(state,performance)};
    if(!row || speed < minimum_envelope_speed || speed >= maximum_envelope_speed) return 0;
    auto const column{static_cast<unsigned int>((speed - minimum_envelope_speed) / envelope_speed_interval)};
    return std::max<uint16_t>(3,envelope_value(context.turn_envelope,performance,*row,column));
  }
  return std::min<uint16_t>(maximum_turn,static_cast<uint16_t>(performance.turn_dividend / (speed | 1)));
}

uint16_t cruise_speed(air_state const &state, air_performance const &performance, std::span<uint8_t const> const envelope) {
  /// Native 0xa4c20 keeps the nineteenth column as the preferred speed index, in increments of 75.
  auto const row{altitude_row(state,performance)};
  return row ? static_cast<uint16_t>(envelope_value(envelope,performance,*row,envelope_columns - 1) * envelope_speed_interval) : 400;
}

void command_speed(air_state &state, air_performance const &performance, int32_t const requested) {
  auto const fixed{[](uint16_t const value) { return std::bit_cast<int32_t>(static_cast<uint32_t>(value) << speed_fraction_bits); }};
  auto const minimum{fixed(performance.minimum_speed)}, maximum{fixed(performance.maximum_speed)};
  // Native 0x75274 has a defined first-match order even when an incompatible profile supplies inverted bounds.
  auto const target{requested <= minimum ? minimum : requested >= maximum ? maximum : requested};
  state.acceleration = std::clamp(wrap(static_cast<int64_t>(target) - state.pose.speed) / air_control_horizon,-performance.deceleration,performance.acceleration);
  reduce_ticks(state,air_control_horizon);
  if(state.pose.speed >= fixed(performance.afterburner_speed)) state.pose.flags |= std::to_underlying(air_flag::afterburner);
}

void command_pitch(air_state &state, air_performance const &performance, math::angle const pitch, air_control_context const &context) {
  /// Native 0x75e94 / 0x75e24: control error wraps as a word, then gets a small-error response adjustment before limiting.
  int16_t const error{word(pitch - state.pose.angles.x)};
  int const adjusted{small_angle(error,0x71c0) ? error + (error >> 3) : error};
  int const limit{maximum_turn_rate(state,performance,context)};
  state.angular_rate.x = static_cast<int16_t>(std::clamp(word(adjusted) / air_control_horizon,-limit,limit));
  reduce_ticks(state,air_control_horizon);
}

bool command_turn(air_state &state, air_performance const &performance, int16_t const error, air_control_context const &context) {
  /// Native 0x75fb4: heading change is coupled to bank; roll toward the requested bank, shortening the next controller interval near completion.
  int constexpr negligible_error{91}, turn_response_threshold{0x6388}, roll_response_threshold{0x71c0}, maximum_bank{0x3c6e}, bank_per_turn_rate{110};
  int requested{error};
  if(small_angle(error,negligible_error)) requested = 0;
  else if(small_angle(error,turn_response_threshold)) requested = word(error + (error >> 2));
  int const turn_rate{std::min(std::abs(word(requested) / air_control_horizon),static_cast<int>(maximum_turn_rate(state,performance,context)))};
  state.angular_rate.y = static_cast<int16_t>(turn_rate);
  int const desired_bank{std::clamp(performance.envelope ? -2 * requested : (requested > 0 ? -turn_rate : turn_rate) * bank_per_turn_rate,-maximum_bank,maximum_bank)};
  int16_t const error_bank{word(desired_bank - state.pose.angles.z)};
  int16_t const adjusted_bank{word(small_angle(error_bank,roll_response_threshold) ? error_bank + (error_bank >> 3) : error_bank)};
  int const roll_rate{std::clamp(adjusted_bank / air_control_horizon,-static_cast<int>(performance.maximum_roll_rate),static_cast<int>(performance.maximum_roll_rate))};
  state.angular_rate.z = static_cast<int16_t>(roll_rate);
  if(!roll_rate && adjusted_bank) {
    state.angular_rate.z = adjusted_bank < 0 ? -1 : 1;
    reduce_ticks(state,word(std::abs(static_cast<int>(adjusted_bank))));
  }
  reduce_ticks(state,air_control_horizon);
  return std::abs(requested) > turn_rate * bank_per_turn_rate;
}

int16_t altitude_pitch(air_state const &state, air_performance const &performance, int32_t const requested) {
  /// Native 0x75ee0 / 0x75ea8: altitude is Q8 world position (32 units per foot), with a 25-foot capture band.
  auto const lower{-static_cast<int32_t>(performance.maximum_altitude) * 32}, upper{-static_cast<int32_t>(performance.minimum_altitude) * 32};
  auto const target{requested < lower ? lower : requested > upper ? upper : requested};
  int32_t const error{wrap(static_cast<int64_t>(target) - state.pose.position.y)};
  int32_t const magnitude{error < 0 ? wrap(-static_cast<int64_t>(error)) : error};
  if(magnitude < 800) return 0;
  int constexpr pitch_bias{32}, minimum_pitch{64}, maximum_pitch{0x38e0};
  int32_t pitch{wrap(static_cast<int64_t>(magnitude / ((state.pose.speed >> 20) | 1)) + pitch_bias)};
  if(pitch > maximum_pitch) pitch = maximum_pitch;
  else if(pitch < minimum_pitch) pitch = 0;
  return word(error > 0 ? pitch : -pitch);
}

void shorten_air_step(air_state &state, int32_t const distance) {
  int const ticks{distance / ((state.pose.speed >> speed_fraction_bits) | 1)};
  reduce_ticks(state,std::clamp(ticks,1,air_control_horizon));
}

std::optional<air_visual_event> advance_air_motion(air_state &state) {
  /// Native 0x77f50 after the controller yields: bank, coupled heading/pitch, speed and Q8 translation, then afterburner visual edge.
  auto &pose{state.pose};
  pose.angles.z = static_cast<math::angle>(pose.angles.z + state.angular_rate.z);
  int32_t const negative_bank{-static_cast<int32_t>(pose.angles.z)};
  state.input_ticks = word(state.input_ticks - 1);
  pose.angles.y = static_cast<math::angle>(pose.angles.y + bank_component(state.angular_rate.y,static_cast<int16_t>(math::sine_q15(negative_bank))));
  pose.angles.x = static_cast<math::angle>(pose.angles.x + bank_component(state.angular_rate.x,static_cast<int16_t>(math::cosine_q15(pose.angles.z))));
  pose.speed = wrap(static_cast<int64_t>(pose.speed) + state.acceleration);
  move(pose.position,pose.angles,pose.speed >> speed_fraction_bits);
  bool const burning{(pose.flags & std::to_underlying(air_flag::afterburner)) != 0};
  bool const displayed{(pose.flags & std::to_underlying(air_flag::afterburner_display)) != 0};
  if(burning == displayed) return {};
  pose.flags &= static_cast<uint16_t>(~std::to_underlying(air_flag::afterburner_display));
  if(burning) pose.flags |= std::to_underlying(air_flag::afterburner_display);
  return burning ? air_visual_event::afterburner_on : air_visual_event::afterburner_off;
}

} // namespace tfx::actors
