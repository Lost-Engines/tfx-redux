#include "aerodynamic_wind.h"
#include <bit>
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr native_pi{3.14159}, inverse_degrees{.005555555555555555};
double constexpr inverse_random_max{1.5259021896696422e-05};
double constexpr wind_smoothing{.05}, renewed_speed_scale{.515};
double constexpr terrain_boost{.1}, weather_boost{.5}, minimum_strong_wind{10};
double constexpr renewal_range{180}, renewal_minimum{60}, initial_renewal{120};
double constexpr upper_wind_start_down{-6000}, upper_wind_full_down{-10000};
double constexpr upper_wind_height{6000}, upper_wind_gradient{-.00025};
double constexpr legacy_gust_scale{7.965905922651053e-06};
unsigned int constexpr data_object_file{973476};

calculation wide(double const value) { return value; }

} // anonymous namespace

std::array<aerodynamic_wind_parameters,aerodynamic_weather_profiles> load_aerodynamic_wind_parameters(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  std::array<aerodynamic_wind_parameters,aerodynamic_weather_profiles> result;
  for(unsigned int index{0}; index != result.size(); ++index) {
    auto const value{[&](unsigned int const field) {
      auto const at{data_object_file + field + index * 8};
      return std::bit_cast<double>(static_cast<uint64_t>(source.le32(at)) | (static_cast<uint64_t>(source.le32(at + 4)) << 32));
    }};
    result[index] = {
      .direction_wobble_scale{value(0x8e40)},
      .surface_speed{value(0x8e68)},
      .upper_speed{value(0x8e90)},
      .upper_direction_degrees{value(0x8eb8)},
      .legacy_gust_period{value(0x8ee0)},
      .legacy_gust_range{value(0x8f08)},
    };
  }
  return result;
}

aerodynamic_surface_temperatures load_aerodynamic_surface_temperatures(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  aerodynamic_surface_temperatures result;
  unsigned int constexpr temperature_table{data_object_file + 0x8c98};
  for(unsigned int profile{0}; profile != result.size(); ++profile) {
    for(unsigned int column{0}; column != result[profile].size(); ++column) result[profile][column] = math::signed_word(source.le16(temperature_table + (profile * aerodynamic_temperature_columns + column) * 2));
  }
  return result;
}

void initialise_aerodynamic_wind(aerodynamic_wind &state, aerodynamic_wind_parameters const &parameters, math::random_state &random, aerodynamic_wind_context const &context, bool const strongest_weather) {
  state.simulation_seconds = 0;
  state.renewal_after = initial_renewal;
  state.direction_wobble_scale = parameters.direction_wobble_scale;
  state.direction_phase_frequency = .04;
  state.direction_phase_offset = 5;
  state.direction_envelope_frequency = 1;
  state.vertical_phase_frequency = .6;
  state.vertical_phase_offset = 6;
  state.target_speed = static_cast<double>(wide(random.next()) * parameters.surface_speed * inverse_random_max);
  if(context.terrain_wind_boost) state.target_speed = static_cast<double>(wide(parameters.surface_speed) * terrain_boost + state.target_speed);
  if(context.weather_wind_boost) state.target_speed = static_cast<double>(wide(parameters.surface_speed) * weather_boost + state.target_speed);
  if(strongest_weather && state.target_speed < minimum_strong_wind) state.target_speed = minimum_strong_wind;
  state.legacy_gust_period = parameters.legacy_gust_period;
  state.legacy_gust_target = static_cast<double>(wide(random.next()) * parameters.legacy_gust_range * legacy_gust_scale);
  state.legacy_gust_smoothing = .08;
  state.velocity_ned = vec3f{0,0,0};
  state.direction = static_cast<double>(wide(random.next()) * (native_pi * 2) * inverse_random_max);
  calculation const upper_angle{wide(native_pi) * parameters.upper_direction_degrees * inverse_degrees};
  state.upper_wind.x = static_cast<double>(std::cos(upper_angle) * parameters.upper_speed);
  state.upper_wind.y = static_cast<double>(std::sin(upper_angle) * parameters.upper_speed);
  state.vertical_angle = 0;
}

void update_aerodynamic_wind(aerodynamic_wind &state, aerodynamic_wind_parameters const &parameters, math::random_state &random, aerodynamic_wind_context const &context) {
  state.smooth_speed = static_cast<double>((wide(state.target_speed) - state.smooth_speed) * wind_smoothing + state.smooth_speed);
  state.velocity_ned.x = static_cast<float>(std::cos(state.direction) * state.smooth_speed);
  state.velocity_ned.y = static_cast<float>(std::sin(state.direction) * state.smooth_speed);
  calculation const time{state.simulation_seconds};
  calculation const direction{std::sin(wide(state.direction_phase_frequency) * time + state.direction_phase_offset) * state.direction_wobble_scale};
  auto const envelope{std::sin(wide(state.direction_envelope_frequency) * time)};
  state.direction = static_cast<double>(direction * envelope + state.direction);
  calculation const vertical{std::sin(wide(state.vertical_phase_frequency) * time + state.vertical_phase_offset) * state.vertical_scale * envelope};
  state.vertical_angle = static_cast<double>(vertical);
  state.velocity_ned.z = static_cast<float>(std::sin(vertical) * state.smooth_speed);
  if(time > state.renewal_after) {
    double const reference{static_cast<double>(wide(parameters.surface_speed) * renewed_speed_scale)};
    state.target_speed = static_cast<double>(wide(random.next()) * reference * inverse_random_max);
    if(context.terrain_wind_boost) state.target_speed = static_cast<double>(wide(parameters.surface_speed) * renewed_speed_scale * terrain_boost + state.target_speed);
    if(context.weather_wind_boost) state.target_speed = static_cast<double>(wide(parameters.surface_speed) * renewed_speed_scale * weather_boost + state.target_speed);
    state.simulation_seconds = 0;
    state.renewal_after = static_cast<double>(wide(random.next()) * renewal_range * inverse_random_max + renewal_minimum);
  }
  if(context.down_metres < upper_wind_start_down) {
    state.upper_mix = context.down_metres > upper_wind_full_down ? static_cast<double>((upper_wind_height + wide(context.down_metres)) * upper_wind_gradient) : 1;
    state.velocity_ned.x = static_cast<float>(wide(state.upper_wind.x) * state.upper_mix + state.velocity_ned.x);
    state.velocity_ned.y = static_cast<float>(wide(state.upper_mix) * state.upper_wind.y + state.velocity_ned.y);
  }
}

} // namespace tfx::flight
