#pragma once

#include "math/native.h"
#include "resources/binary.h"

namespace tfx::flight {

unsigned int constexpr aerodynamic_weather_profiles{5}, aerodynamic_temperature_columns{4};
// Native 0x95f3c explicitly starts in profile zero; the temperature column is
// initialized to zero and has no referenced writer in the reference executable.
unsigned int constexpr aerodynamic_initial_weather{0}, aerodynamic_initial_temperature_column{0};
using aerodynamic_surface_temperatures = std::array<std::array<int16_t,aerodynamic_temperature_columns>,aerodynamic_weather_profiles>;
struct aerodynamic_wind_parameters {
  double direction_wobble_scale{0};
  double surface_speed{0};
  double upper_speed{0};
  double upper_direction_degrees{0};
  double legacy_gust_period{0};
  double legacy_gust_range{0};
};
struct aerodynamic_wind {
  double simulation_seconds{0};                                                // Shared original flight clock; gust renewal resets it.
  double smooth_speed{0};
  double target_speed{0};
  double direction{0};
  double vertical_angle{0};
  double renewal_after{0};
  double direction_wobble_scale{0};
  double direction_phase_frequency{0};
  double direction_phase_offset{0};
  double direction_envelope_frequency{0};
  double vertical_phase_frequency{0};
  double vertical_phase_offset{0};
  double vertical_scale{0};                                                    // Compiled zero, with no native writer.
  vec2d upper_wind{0,0};
  double upper_mix{0};
  double legacy_gust_period{0};
  double legacy_gust_target{0};
  double legacy_gust_smoothing{0};                                             // Three initialized fields have no native readers.
  vec3f velocity_ned{0,0,0};
};
struct aerodynamic_wind_context {
  double down_metres{0};
  bool terrain_wind_boost{false};                                              // Native 0xe762 != 0.
  bool weather_wind_boost{false};                                              // Native environment flag 0x80.
};

std::array<aerodynamic_wind_parameters,aerodynamic_weather_profiles> load_aerodynamic_wind_parameters(std::span<uint8_t const> executable);
aerodynamic_surface_temperatures load_aerodynamic_surface_temperatures(std::span<uint8_t const> executable);
// Original 0x95f3c wind initialization, including all three random draws.
void initialise_aerodynamic_wind(aerodynamic_wind &state, aerodynamic_wind_parameters const &parameters, math::random_state &random, aerodynamic_wind_context const &context, bool strongest_weather);
// Complete 0x996c0. The caller advances the shared clock separately.
void update_aerodynamic_wind(aerodynamic_wind &state, aerodynamic_wind_parameters const &parameters, math::random_state &random, aerodynamic_wind_context const &context);

} // namespace tfx::flight
