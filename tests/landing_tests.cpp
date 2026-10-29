#include "fixture_check.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "flight/landing.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"landing tests require the native fixture"};
  std::ifstream input{argv[1]};
  if(!input) throw std::runtime_error{"cannot open native ILS fixture"};
  unsigned int index,count{0};
  while(input >> index) {
    tfx::flight::landing_state initial;unsigned int height;
    input >> initial.enabled >> initial.carrier >> initial.beacon >> initial.cell.x >> initial.cell.y >> initial.port.offset.x >> initial.port.offset.y >> initial.port.heading >> initial.beacon_position.x >> initial.beacon_position.y >> initial.bearing.x >> initial.bearing.y >> initial.correction.x >> initial.correction.y >> initial.pitch_error >> height;
    initial.height = static_cast<tfx::flight::approach_height>(height);
    tfx::flight::landing_system controller{std::move(initial)};
    vec3<int32_t> player;vec2<int16_t> offset;bool speed;
    tfx::flight::landing_projection projection;
    input >> player.x >> player.y >> player.z >> speed >> projection.valid >> projection.point.x >> projection.point.y >> offset.x >> offset.y;
    vec3<int32_t> projected;
    auto const cue{controller.advance(player,speed,[&](auto const &point) {
      for(unsigned int axis{0}; axis != 3; ++axis) projected[axis] = tfx::math::signed_word(static_cast<uint16_t>(point[axis]));
      return projection;
    })};
    auto const &state{controller.value()};
    std::array<int32_t,20> const actual{state.enabled,state.carrier,state.beacon,state.cell.x,state.cell.y,state.port.offset.x,state.port.offset.y,state.port.heading,state.beacon_position.x,state.beacon_position.y,state.bearing.x,state.bearing.y,state.correction.x,state.correction.y,state.pitch_error,static_cast<int32_t>(std::to_underlying(state.height)),cue.speed_request,projected.x,projected.y,projected.z};
    for(auto const value : actual) {
      if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native ILS fixture differs at case " + std::to_string(index)};
    }
    ++count;
  }
  if(!count) throw std::runtime_error{"empty native ILS fixture"};
  std::cout << count << " original ILS state fixtures passed\n";
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
