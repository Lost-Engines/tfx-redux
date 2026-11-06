#include <iostream>
#include <string_view>
#include "render/horizon.h"
#include "flight/aerodynamic_launch.h"
#include "flight/player_flight.h"
#include "flight/player_shared.h"
#include "flight/aerodynamic_pilot_keys.h"
#include "cockpit/initialisation.h"
#include "weapons/definition.h"
#include "aerodynamic_autopilot_snapshot.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 4) throw std::runtime_error{"expected TFX.DAT [--model-changes] [--horizon]"};
  bool model_changes{false},horizon{false};
  for(int i{2}; i != argc; ++i) {
    auto const argument{std::string_view{argv[i]}};
    if(argument == "--model-changes") model_changes = true;
    else if(argument == "--horizon") horizon = true;
    else throw std::runtime_error{"unknown player probe option"};
  }
  auto const source{tfx::resources::read_file(argv[1])};
  auto const profiles{tfx::flight::load_aerodynamic_airframes(source)};
  auto const wind{tfx::flight::load_aerodynamic_wind_parameters(source)};
  auto const weapons{tfx::weapons::load_definitions(source)};
  unsigned int craft,model,landed,route_count,slot,weapon,occupied,low,high; int height;
  while(std::cin >> craft >> model >> landed >> height >> route_count >> slot >> weapon >> occupied >> low >> high) {
    tfx::flight::aerodynamic_launch_context context{
      .model{aerodynamic_test_model(model)},
      .player_route_points{route_count},
      .ground_elevation_steps{static_cast<int16_t>(height)},
      .landed{landed != 0},
    };
    for(auto &value : context.position) std::cin >> value;
    for(auto &value : context.angles) std::cin >> value;
    unsigned int temperature,terrain_boost,weather_boost; int celsius;
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
    damage.systems = tfx::cockpit::initialise_instruments(static_cast<tfx::flight::player_aircraft>(craft),context.landed).systems;
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    tfx::flight::player_flight player;
    tfx::render::horizon_phase ground_phase;
    player.initialise(profile,stores,context,.65f,wind[0],damage,random);
    // This subsystem sequence starts after the separately checked placement prefix.
    tfx::flight::place_aerodynamic_start(player.physical,context.ground_elevation_steps);
    tfx::flight::publish_shared_player(player.basic,player.physical);
    auto &state{player.physical};
    player.fuel = {
      .reading{9999},
      .smoothed_burn{2},
    };
    unsigned int count; std::cin >> count;
    std::vector<vec2<uint8_t>> route;
    for(unsigned int index{0}; index != route_count; ++index) route.emplace_back(static_cast<uint8_t>(30 + index),static_cast<uint8_t>(40 + index));
    for(unsigned int tick{0}; tick != count; ++tick) {
      uint64_t seconds;
      unsigned int elapsed,keys,accelerate,decelerate,buttons,source,action,refuelling,disabled,gear,engines,left_condition,right_condition,landing_phase;
      int pitch,roll,joyx,joyy;
      std::cin >> elapsed >> seconds >> keys >> accelerate >> decelerate >> buttons >> source >> action >> refuelling >> disabled >> gear >> pitch >> roll >> joyx >> joyy >> engines >> left_condition >> right_condition >> landing_phase;
      if(model_changes) { unsigned int next;std::cin >> next;player.select_model(static_cast<tfx::flight::aerodynamic_model>(next)); }
      if(gear != 255) {
        player.basic.flight.gear_down = gear != 0;
        damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = gear ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal;
      }
      if(engines != 255) {
        player.basic.flight.left_engine = (engines & 1) != 0;
        player.basic.flight.right_engine = (engines & 2) != 0;
      }
      if(left_condition != 255) damage.systems[std::to_underlying(tfx::flight::aircraft_system::left_engine)] = static_cast<tfx::flight::system_condition>(left_condition);
      if(right_condition != 255) damage.systems[std::to_underlying(tfx::flight::aircraft_system::right_engine)] = static_cast<tfx::flight::system_condition>(right_condition);
      if(action != 255) {
        tfx::flight::capture_shared_player(state,player.basic,damage,player.simple_body);
        tfx::flight::press_aerodynamic_pilot_key(state,damage,player.stick_pressure,static_cast<tfx::flight::aerodynamic_pilot_key>(action),{
          .ground_elevation_steps{context.ground_elevation_steps},
          .terrain_present{true},
          .ejected{disabled != 0},
          .recovery_available{true},
        });
        tfx::flight::publish_shared_player(player.basic,state);
      }
      if(landing_phase != 255) player.automatic.phase = static_cast<tfx::flight::automatic_landing_phase>(landing_phase);
      tfx::flight::landing_state runway{
        .cell{30,40},
        .port{},
      };
      auto const events{player.advance(damage,random,wind[0],{
        .runway{runway},
        .physics{
          .input{
            .source{static_cast<tfx::flight::control_source>(source)},
            .keys{
              .up{(keys & 1) != 0},
              .left{(keys & 2) != 0},
              .down{(keys & 4) != 0},
              .right{(keys & 8) != 0},
            },
            .joystick{static_cast<int16_t>(joyx),static_cast<int16_t>(joyy)},
            .alt{(keys & 16) != 0},
            .accelerate{accelerate != 0},
            .decelerate{decelerate != 0},
          },
          .seconds{std::bit_cast<double>(seconds)},
          .ground_elevation_steps{context.ground_elevation_steps},
          .refuelling_phase{static_cast<uint8_t>(refuelling)},
          .terrain_present{true},
          .crashing{true},
          .stick_pressure{player.stick_pressure},
          .navigation{
            .route_cells{route},
          },
          .recovery_available{true},
        },
        .basic_controls{
          .pitch_delta{static_cast<int16_t>(pitch)},
          .roll_delta{static_cast<int16_t>(roll)},
          .accelerate{accelerate != 0},
          .decelerate{decelerate != 0},
          .mouse_buttons{static_cast<uint8_t>(buttons)},
        },
        .elapsed{static_cast<tfx::simulation::timer_ticks>(elapsed)},
        .aircraft_disabled{disabled != 0},
      })};
      for(auto const value : aerodynamic_snapshot(state,damage,random)) std::cout << value << ' ';
      for(auto const value : aerodynamic_autopilot_snapshot(state.autopilot)) std::cout << value << ' ';
      std::cout << player.fuel.reading << ' ' << player.fuel.smoothed_burn << ' ' << player.measured_speed_q8 << ' ';
      std::cout << std::bit_cast<uint32_t>(player.basic.flight.speed_mach) << ' ' << std::bit_cast<uint32_t>(player.basic.flight.speed_metres_per_second) << ' ' << std::bit_cast<uint32_t>(player.basic.flight.target_speed) << ' ';
      std::cout << std::bit_cast<uint32_t>(player.basic.reported_speed) << ' ';
      std::cout << static_cast<unsigned int>(events.callback) << ' ' << static_cast<unsigned int>(player.automatic.phase) << ' ' << static_cast<uint32_t>(events.landing.score_change);
      if(horizon) { if(events.horizon_motion) ground_phase.advance(*events.horizon_motion);std::cout << ' ' << static_cast<uint32_t>(ground_phase.packed_state()); }
      std::cout << '\n';
    }
  }
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
