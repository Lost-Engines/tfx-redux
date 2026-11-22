#include <bit>
#include <iostream>
#include <utility>
#include "flight/ejection_sequence.h"

int main() {
  unsigned int presses;
  unsigned int stage;
  unsigned int phase;
  while(std::cin >> presses >> stage >> phase) {
    tfx::flight::ejection_state initial;
    initial.escape_presses = static_cast<uint8_t>(presses);
    initial.stage = static_cast<tfx::flight::ejection_stage>(stage);
    initial.pilot.phase = static_cast<tfx::flight::pilot_phase>(phase);
    for(auto &value : initial.aircraft_position_q8) std::cin >> value;
    for(unsigned int row{0}; row != 3; ++row) std::cin >> initial.aircraft_orientation.right[row] >> initial.aircraft_orientation.down[row] >> initial.aircraft_orientation.forward[row];
    vec3<int32_t> position;
    for(auto &value : position) std::cin >> value;
    vec3f floating;
    for(auto &value : floating) {
      uint32_t bits;
      std::cin >> bits;
      value = std::bit_cast<float>(bits);
    }
    tfx::math::attitude orientation;
    for(unsigned int row{0}; row != 3; ++row) std::cin >> orientation.right[row] >> orientation.down[row] >> orientation.forward[row];
    vec3<tfx::math::angle> angles;
    for(auto &value : angles) std::cin >> value;
    int32_t speed;
    unsigned int ticks;
    unsigned int calls;
    bool mark;
    std::cin >> speed >> ticks >> calls >> mark;
    tfx::flight::ejection_sequence sequence{std::move(initial)};
    bool ended{false};
    bool refuelling{true};
    vec3<int32_t> camera{7, 8, 9};
    std::vector<tfx::flight::ejection_event> events;
    for(unsigned int call{0}; call != calls; ++call) {
      auto const result{sequence.escape(position, angles, speed)};
      ended = ended || result.end_flight;
      refuelling = refuelling && !result.disable_refuelling;
      if(result.fixed_camera) camera = vec3<int32_t>{*result.fixed_camera};
      for(auto const &event : result.events) events.push_back(event);
    }
    if(mark) sequence.request();
    bool const activated{sequence.advance_aircraft(floating, orientation, speed, static_cast<tfx::simulation::timer_ticks>(ticks))};
    auto const &state{sequence.value()};
    std::cout << static_cast<unsigned int>(state.escape_presses) << ' ' << static_cast<unsigned int>(state.stage) << ' ' << static_cast<unsigned int>(state.pilot.phase) << ' ' << activated << ' ' << ended << ' ' << refuelling << ' ';
    for(auto const value : camera) std::cout << value << ' ';
    for(auto const value : state.aircraft_position_q8) std::cout << value << ' ';
    for(unsigned int row{0}; row != 3; ++row) std::cout << state.aircraft_orientation.right[row] << ' ' << state.aircraft_orientation.down[row] << ' ' << state.aircraft_orientation.forward[row] << ' ';
    std::cout << events.size();
    for(auto const &event : events) std::cout << ' ' << std::to_underlying(event.kind) << ' ' << event.argument;
    std::cout << '\n';
  }
}
