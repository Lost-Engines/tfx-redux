#pragma once

#include "aerodynamic_autopilot_snapshot.h"
#include "aerodynamic_snapshot.h"
#include "flight/automatic_landing.h"

inline void trace_automatic_landing(tfx::flight::aerodynamic_flight &flight, tfx::flight::damage_state &damage, tfx::math::random_state &random, std::istream &input) {
  unsigned int count;
  input >> count;
  tfx::flight::automatic_landing landing;
  for(unsigned int tick{0}; tick != count; ++tick) {
    unsigned int phase;
    unsigned int carrier;
    unsigned int inhibited;
    unsigned int ejected;
    unsigned int crashed;
    unsigned int condition;
    unsigned int flare;
    unsigned int heading;
    input >> phase >> carrier >> inhibited >> ejected >> crashed >> condition;
    std::array<uint64_t, 9> values;
    for(auto &value : values) input >> value;
    tfx::flight::landing_state runway;
    input >> runway.cell.x >> runway.cell.y >> runway.port.offset.x >> runway.port.offset.y >> heading;
    runway.port.heading = static_cast<tfx::math::angle>(heading);
    runway.carrier = carrier != 0;
    int height;
    input >> height;
    vec3<int32_t> destination;
    vec3<int32_t> player;
    for(auto *vector : {&destination, &player}) {
      for(auto &value : *vector) input >> value;
    }
    input >> flare;
    if(!input) throw std::runtime_error{"truncated automatic landing trace"};
    flight.seconds = std::bit_cast<double>(values[1]);
    if(phase != 255) {
      landing.phase = static_cast<tfx::flight::automatic_landing_phase>(phase);
      landing.approach.destination_metres = destination;
      landing.approach.player_metres = player;
      landing.approach.distance_km = std::bit_cast<double>(values[5]);
      landing.approach.pitch_degrees = std::bit_cast<double>(values[6]);
      landing.approach.flaring = flare != 0;
      flight.gear_fraction = std::bit_cast<double>(values[0]);
      flight.data.metres_per_second = std::bit_cast<double>(values[2]);
      flight.response.motion.pose.metres_ned.z = std::bit_cast<double>(values[3]);
      flight.response.motion.rotation_remainder = {std::bit_cast<double>(values[7]), std::bit_cast<double>(values[8])};
      flight.response.motion.collision = static_cast<int16_t>(crashed);
      damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = static_cast<tfx::flight::system_condition>(condition);
      flight.powerplant.afterburner = flight.aircraft.powerplant.afterburner_stages ? 2 : 0;
    }
    auto const events{tfx::flight::advance_automatic_landing(landing, flight, damage, {
      .runway{runway},
      .ground_elevation_steps{static_cast<int16_t>(height)},
      .resting_pitch_radians{std::bit_cast<double>(values[4])},
      .inhibited{inhibited != 0},
      .ejected{ejected != 0},
    })};
    for(auto const value : aerodynamic_snapshot(flight, damage, random)) std::cout << value << ' ';
    for(auto const value : aerodynamic_autopilot_snapshot(flight.autopilot)) std::cout << value << ' ';
    std::cout << static_cast<unsigned int>(landing.phase) << ' ';
    for(auto const *vector : {&landing.approach.destination_metres, &landing.approach.player_metres}) {
      for(auto const value : *vector) std::cout << static_cast<uint32_t>(value) << ' ';
    }
    std::cout << std::bit_cast<uint64_t>(landing.approach.distance_km) << ' ' << std::bit_cast<uint64_t>(landing.approach.pitch_degrees) << ' ' << landing.approach.flaring << ' ';
    std::cout << static_cast<uint32_t>(events.score_change) << ' ' << events.clear_basic_speed << ' ' << events.ordered.size() << ' ';
    for(auto const event : events.ordered) std::cout << std::to_underlying(event) << ' ';
    std::cout << '\n';
  }
}
