#include <bit>
#include <cstdlib>
#include <iostream>
#include "flight/aerodynamic_wind.h"
#include "render/precipitation.h"
#include "world/environment.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"weather probe requires original installation"};
  auto const directory{std::filesystem::path{argv[1]}};
  tfx::resources::archive const game{tfx::resources::find_game_archive(directory)};
  auto const executable{tfx::resources::read_file(directory / "TFX.DAT")};
  auto const parameters{tfx::flight::load_aerodynamic_wind_parameters(executable)};
  auto const temperatures{tfx::flight::load_aerodynamic_surface_temperatures(executable)};
  std::cout << "temperatures";
  for(auto const &row : temperatures) {
    for(auto const value : row) std::cout << ' ' << value;
  }
  std::cout << '\n';
  std::string name;
  unsigned int enabled;
  unsigned int low;
  unsigned int high;
  while(std::cin >> name >> enabled >> low >> high) {
    tfx::world::lighting_configuration const environment{game, name};
    tfx::flight::aerodynamic_wind state;
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    tfx::flight::initialise_aerodynamic_wind(state, parameters[tfx::flight::aerodynamic_initial_weather], random, {
      .terrain_wind_boost{environment.cloud_layer},
      .weather_wind_boost{environment.storm},
    }, false);
    double const kelvin{enabled ? temperatures[tfx::flight::aerodynamic_initial_weather][tfx::flight::aerodynamic_initial_temperature_column] + 273. : 288.15};
    std::cout << environment.cloud_layer << ' ' << environment.storm << ' ' << std::bit_cast<uint64_t>(kelvin);
    for(auto const value : {state.simulation_seconds, state.smooth_speed, state.target_speed, state.direction, state.vertical_angle, state.renewal_after, state.direction_wobble_scale, state.direction_phase_frequency, state.direction_phase_offset, state.direction_envelope_frequency, state.vertical_phase_frequency, state.vertical_phase_offset, state.vertical_scale, state.upper_wind.x, state.upper_wind.y, state.upper_mix, state.legacy_gust_period, state.legacy_gust_target, state.legacy_gust_smoothing}) std::cout << ' ' << std::bit_cast<uint64_t>(value);
    for(auto const value : state.velocity_ned) std::cout << ' ' << std::bit_cast<uint32_t>(value);
    if(argc == 3) {
      tfx::render::precipitation rain;
      rain.initialise(random);
      uint64_t hash{14695981039346656037ull};
      for(auto const &point : rain.field()) {
        for(unsigned int axis{0}; axis != 3; ++axis) {
          for(unsigned int shift{0}; shift != 16; shift += 8) hash = (hash ^ static_cast<uint8_t>(static_cast<uint16_t>(point[axis]) >> shift)) * 1099511628211;
        }
      }
      std::cout << ' ' << hash;
    }
    std::cout << ' ' << random.low << ' ' << random.high << '\n';
  }
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
