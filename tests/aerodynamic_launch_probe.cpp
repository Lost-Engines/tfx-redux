#include <cstdlib>
#include <iostream>
#include "aerodynamic_autopilot_snapshot.h"
#include "cockpit/initialisation.h"
#include "flight/aerodynamic_launch.h"
#include "weapons/definition.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"expected TFX.DAT"};
  auto const source{tfx::resources::read_file(argv[1])};
  auto const profiles{tfx::flight::load_aerodynamic_airframes(source)};
  auto const wind{tfx::flight::load_aerodynamic_wind_parameters(source)};
  auto const weapons{tfx::weapons::load_definitions(source)};
  unsigned int craft;
  unsigned int model;
  unsigned int landed;
  unsigned int route_count;
  unsigned int slot;
  unsigned int weapon;
  unsigned int occupied;
  unsigned int low;
  unsigned int high;
  int height;
  while(std::cin >> craft >> model >> landed >> height >> route_count >> slot >> weapon >> occupied >> low >> high) {
    tfx::flight::aerodynamic_launch_context context{
      .model{aerodynamic_test_model(model)},
      .player_route_points{route_count},
      .ground_elevation_steps{static_cast<int16_t>(height)},
      .landed{landed != 0},
    };
    for(auto &value : context.position) std::cin >> value;
    for(auto &value : context.angles) std::cin >> value;
    unsigned int temperature;
    unsigned int terrain_boost;
    unsigned int weather_boost;
    int celsius;
    std::cin >> temperature >> celsius >> terrain_boost >> weather_boost;
    context.wind.terrain_wind_boost = terrain_boost != 0;
    context.wind.weather_wind_boost = weather_boost != 0;
    if(temperature) context.surface_celsius = static_cast<int16_t>(celsius);
    auto const &profile{profiles.at(craft - 1)};
    tfx::flight::loadout stores;
    if(occupied) {
      auto const &definition{weapons.at(weapon)};
      stores.at(slot) = {
        .position{vec3d{profile.stores_and_mounts.mounts.at(slot)}},
        .properties{
          .mass_kg{definition.mass},
          .drag{definition.drag},
          .fuel_tank{definition.role == tfx::weapons::weapon_role::external_tank},
        },
        .occupied{true},
      };
    }
    tfx::flight::damage_state damage;
    damage.systems = tfx::cockpit::initialise_instruments(static_cast<tfx::flight::player_aircraft>(craft), context.landed).systems;
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    auto state{tfx::flight::launch_aerodynamic_flight(profile, stores, context, wind[0], damage, random)};
    if(argc == 2) tfx::flight::place_aerodynamic_start(state, context.ground_elevation_steps);
    for(auto const value : aerodynamic_snapshot(state, damage, random)) std::cout << value << ' ';
    for(auto const value : aerodynamic_autopilot_snapshot(state.autopilot)) std::cout << value << ' ';
    std::cout << state.cold_start << ' ' << static_cast<unsigned int>(state.autopilot.saved_air_brake) << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
