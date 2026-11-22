#include <bit>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "aerodynamic_autopilot_snapshot.h"
#include "aerodynamic_pilot_keys_capture.h"
#include "aerodynamic_snapshot.h"
#include "automatic_landing_capture.h"
#include "flight/aerodynamic_step.h"
#include "weapons/definition.h"

int main(int const argc, char const *const argv[]) try {
  bool const pilot_key{argc == 3 && std::string_view{argv[2]} == "--pilot-key"};
  bool const landing_trace{argc == 3 && std::string_view{argv[2]} == "--landing-trace"};
  bool const autopilot_trace{argc == 3 && std::string_view{argv[2]} == "--autopilot-trace"};
  bool const trace{autopilot_trace || (argc == 3 && std::string_view{argv[2]} == "--trace")};
  if(argc != 2 && !trace && !landing_trace && !pilot_key) throw std::runtime_error{"expected TFX.DAT [--trace|--autopilot-trace|--landing-trace]"};
  auto const source{tfx::resources::read_file(argv[1])};
  auto const frames{tfx::flight::load_aerodynamic_airframes(source)};
  auto const wind{tfx::flight::load_aerodynamic_wind_parameters(source)};
  auto const weapons{tfx::weapons::load_definitions(source)};
  unsigned int craft,model,landed,cold,terrain,boost,terrain_boost,gear,slot,weapon,occupied,low,high;
  int height,fixed_speed;
  while(std::cin >> craft >> model >> landed >> cold >> terrain >> boost >> terrain_boost >> gear >> slot >> weapon >> occupied >> low >> high >> height >> fixed_speed) {
    tfx::flight::aerodynamic_start_context context{
      .fixed_speed{fixed_speed},
      .ground_elevation_steps{static_cast<int16_t>(height)},
      .landed{landed != 0},
      .cold_start{cold != 0},
      .terrain_present{terrain != 0},
    };
    for(auto &value : context.position) {
      uint32_t bits; std::cin >> bits;
      value = std::bit_cast<float>(bits);
    }
    for(unsigned int row{0}; row != 3; ++row) std::cin >> context.orientation.right[row] >> context.orientation.down[row] >> context.orientation.forward[row];
    unsigned int temperature; int celsius;
    std::cin >> temperature >> celsius;
    context.heading = context.orientation.angles().y;
    tfx::flight::damage_state damage;
    damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = static_cast<tfx::flight::system_condition>(gear);
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    tfx::flight::loadout stores;
    if(occupied) {
      auto const &definition{weapons.at(weapon)};
      stores.at(slot) = {
        .position{vec3d{frames.at(craft - 1).stores_and_mounts.mounts.at(slot)}},
        .properties{
          .mass_kg{definition.mass},
          .drag{definition.drag},
          .fuel_tank{definition.role == tfx::weapons::weapon_role::external_tank},
        },
        .occupied{true},
      };
    }
    tfx::flight::aerodynamic_wind_context const wind_context{
      .terrain_wind_boost{terrain_boost != 0},
      .weather_wind_boost{boost != 0},
    };
    tfx::flight::aerodynamic_initial_context const initial{
      .placement{context},
      .model{aerodynamic_test_model(model)},
      .wind{wind_context},
      .surface_celsius{temperature ? std::optional<int16_t>{static_cast<int16_t>(celsius)} : std::nullopt},
    };
    auto state{tfx::flight::initialise_aerodynamic_flight(frames.at(craft - 1),stores,initial,wind[0],damage,random)};
    for(auto const value : aerodynamic_snapshot(state,damage,random)) std::cout << value << ' ';
    std::cout << '\n';
    if(pilot_key) capture_aerodynamic_pilot_key(state,damage,random,std::cin,static_cast<int16_t>(height),terrain != 0);
    if(landing_trace) trace_automatic_landing(state,damage,random,std::cin);
    if(trace) {
      unsigned int count; std::cin >> count;
      for(unsigned int tick{0}; tick != count; ++tick) {
        uint64_t seconds; unsigned int source,up,left,down,right,alt,accelerate,decelerate,wind_enabled,warp,pressure,braking,crashing,wet,gear_damage,surface;
        int joystick_x,joystick_y,mouse_x,mouse_y;
        if(!(std::cin >> seconds >> source >> up >> left >> down >> right >> alt >> accelerate >> decelerate >> joystick_x >> joystick_y >> mouse_x >> mouse_y >> wind_enabled >> warp >> pressure >> braking >> crashing >> wet >> gear_damage >> surface)) throw std::runtime_error{"truncated aerodynamic trace input"};
        tfx::flight::aerodynamic_navigation_context navigation;
        std::vector<vec2<uint8_t>> cells;
        unsigned int ils{0},callback{0};
        if(autopilot_trace) {
          unsigned int mode,enabled,heading,pitch,count,followed; int route_index;
          uint64_t target_down,target_speed;
          std::cin >> mode >> enabled >> heading >> target_down >> target_speed >> route_index >> pitch >> followed >> count >> ils >> callback;
          auto &targets{state.manual.controls.autopilot};
          if(mode == 65534) {
            // A key-triggered request precedes the callback, unlike automatic
            // recovery raised during its air-data stage.
            tfx::flight::toggle_aerodynamic_recovery(state.autopilot.recovery,state.manual.controls,state.powerplant,state.manual.air_brake_condition,{
              .afterburner_stages{state.aircraft.powerplant.afterburner_stages},
              .available{true},
              .crashed{state.response.motion.collision != 0},
              .landed{state.ground.ground.landed},
            });
          } else if(mode != 65535) {
            targets.mode = static_cast<uint16_t>(mode);
            targets.enabled = enabled != 0;
            targets.heading = static_cast<tfx::math::angle>(heading);
            targets.down_metres = std::bit_cast<double>(target_down);
            targets.metres_per_second = std::bit_cast<double>(target_speed);
          }
          if(route_index != -32768) {
            state.autopilot.navigation.route_index = static_cast<int16_t>(route_index);
            state.autopilot.navigation.target_pitch = static_cast<tfx::math::angle>(pitch);
          }
          for(unsigned int cell{0}; cell != count; ++cell) {
            unsigned int x,z; std::cin >> x >> z;
            cells.emplace_back(static_cast<uint8_t>(x),static_cast<uint8_t>(z));
          }
          navigation.route_cells = cells;
          tfx::flight::aerodynamic_follow_target target;
          for(auto &value : target.position_q8) std::cin >> value;
          std::cin >> target.speed_q16;
          if(followed) navigation.followed_aircraft.emplace(target);
        }
        tfx::flight::aerodynamic_tick_context const step{
          .input{
            .source{static_cast<tfx::flight::control_source>(source)},
            .keys{
              .up{up != 0},
              .left{left != 0},
              .down{down != 0},
              .right{right != 0},
            },
            .joystick{static_cast<int16_t>(joystick_x),static_cast<int16_t>(joystick_y)},
            .mouse{static_cast<int16_t>(mouse_x),static_cast<int16_t>(mouse_y)},
            .alt{alt != 0},
            .accelerate{accelerate != 0},
            .decelerate{decelerate != 0},
          },
          .seconds{std::bit_cast<double>(seconds)},
          .ground_elevation_steps{static_cast<int16_t>(height)},
          .surface_celsius{initial.surface_celsius},
          .terrain_present{terrain != 0},
          .rough_surface{surface == 2},
          .carrier_surface{surface == 3},
          .wheel_brakes{braking != 0},
          .gear_damage_enabled{gear_damage != 0},
          .precipitation{wet != 0},
          .time_warp{warp != 0},
          .crashing{crashing != 0},
          .stick_pressure{pressure != 0},
          .wind_enabled{wind_enabled != 0},
          .terrain_wind_boost{terrain_boost != 0},
          .weather_wind_boost{boost != 0},
          .navigation{navigation},
          .recovery_available{true},
          .approach_reference_enabled{ils != 0},
          .callback{static_cast<tfx::flight::aerodynamic_callback>(callback)},
        };
        auto const events{tfx::flight::advance_aerodynamic_flight(state,damage,random,wind[0],step)};
        for(auto const value : aerodynamic_snapshot(state,damage,random)) std::cout << value << ' ';
        if(autopilot_trace) {
          for(auto const value : aerodynamic_autopilot_snapshot(state.autopilot)) std::cout << value << ' ';
          for(auto const value : {state.approach.flap_fraction,state.approach.lift_coefficient,state.approach.angle_of_attack,state.approach.flight_path_degrees,state.approach.ground_speed}) std::cout << std::bit_cast<uint64_t>(value) << ' ';
        }
        std::cout << events.ground.gear_impacts << ' ' << events.ground.cockpit_bump << ' ' << events.afterburner.size() << ' ';
        for(auto const change : events.afterburner) std::cout << std::to_underlying(change) << ' ';
        if(autopilot_trace) {
          std::cout << events.recovery_changes.size() << ' ';
          for(auto const change : events.recovery_changes) std::cout << std::to_underlying(change) << ' ';
          std::cout << events.descent_warning << ' ';
        }
        std::cout << '\n';
        if(autopilot_trace) state.autopilot.recovery.model_reset_requested = false; // Native outer caller 0x8f98c.
      }
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
