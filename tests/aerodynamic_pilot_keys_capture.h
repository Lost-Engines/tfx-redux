#pragma once

#include "aerodynamic_autopilot_snapshot.h"
#include "flight/aerodynamic_pilot_keys.h"

inline void capture_aerodynamic_pilot_key(tfx::flight::aerodynamic_flight &state, tfx::flight::damage_state &damage, tfx::math::random_state &random, std::istream &input, int16_t const height, bool const terrain) {
  unsigned int key;
  unsigned int pressure;
  unsigned int inhibited;
  unsigned int ejected;
  unsigned int crashed;
  unsigned int available;
  unsigned int enabled;
  unsigned int mode;
  unsigned int heading;
  unsigned int brake;
  unsigned int saved_brake;
  unsigned int afterburner;
  unsigned int view_height;
  int track;
  uint64_t speed;
  uint64_t down;
  uint64_t physical_speed;
  uint64_t sound_speed;
  input >> key >> pressure >> inhibited >> ejected >> crashed >> available >> enabled >> mode >> heading >> brake >> saved_brake >> afterburner >> speed >> down >> physical_speed >> view_height >> track >> sound_speed;
  if(!input) throw std::runtime_error{"truncated aerodynamic pilot key input"};
  auto &targets{state.manual.controls.autopilot};
  targets.enabled = enabled != 0;
  targets.mode = static_cast<uint16_t>(mode);
  targets.heading = static_cast<tfx::math::angle>(heading);
  targets.metres_per_second = std::bit_cast<double>(speed);
  targets.down_metres = std::bit_cast<double>(down);
  state.powerplant.afterburner = static_cast<uint8_t>(afterburner);
  state.response.motion.collision = static_cast<int16_t>(crashed);
  state.seconds = .1;
  state.data.atmosphere.sound_metres_per_second = std::bit_cast<double>(sound_speed);
  state.data.metres_per_second = std::bit_cast<double>(physical_speed);
  state.view.position.y = std::bit_cast<float>(view_height);
  state.data.track_heading = track;
  state.manual.air_brake_condition = static_cast<tfx::flight::system_condition>(brake);
  damage.systems[std::to_underlying(tfx::flight::aircraft_system::air_brake)] = state.manual.air_brake_condition;
  state.autopilot.saved_air_brake = static_cast<tfx::flight::system_condition>(saved_brake);
  bool stick_pressure{pressure != 0};
  auto const events{tfx::flight::press_aerodynamic_pilot_key(state, damage, stick_pressure, static_cast<tfx::flight::aerodynamic_pilot_key>(key), {
    .ground_elevation_steps{height},
    .terrain_present{terrain},
    .inhibited{inhibited != 0},
    .ejected{ejected != 0},
    .recovery_available{available != 0},
  })};
  for(auto const value : aerodynamic_snapshot(state, damage, random)) std::cout << value << ' ';
  for(auto const value : aerodynamic_autopilot_snapshot(state.autopilot)) std::cout << value << ' ';
  std::cout << static_cast<unsigned int>(state.autopilot.saved_air_brake) << ' ' << stick_pressure << ' ';
  std::cout << (events.notice ? static_cast<unsigned int>(*events.notice) + 1 : 0) << ' ' << static_cast<unsigned int>(events.external_status) << ' ' << static_cast<unsigned int>(events.recovery.change) << ' ' << events.motion_reset << ' ' << events.recovery.afterburner.size() << ' ';
  for(auto const change : events.recovery.afterburner) std::cout << static_cast<unsigned int>(change) << ' ';
  std::cout << '\n';
}
