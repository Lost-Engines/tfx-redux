#include "fixture_check.h"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include "flight/simple_ground.h"

namespace {

float read_float(std::istream &input) {
  uint32_t bits;input >> bits;
  return std::bit_cast<float>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open Simple ground fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0}, action;
  while(input >> action) {
    tfx::flight::simple_ground_state state;
    state.position_y = read_float(input);
    input >> state.stall_pitch >> state.landing_angles.x >> state.landing_angles.y >> state.landed >> state.previous_landed >> state.low_speed_ground_contact >> state.collision >> state.arrested;
    for(auto *basis : {&state.body,&state.visible}) for(unsigned int row{0}; row != 3; ++row) input >> basis->right[row] >> basis->down[row] >> basis->forward[row];
    for(auto &value : state.rotation_remainder) value = read_float(input);
    tfx::flight::simple_ground_context context;
    unsigned int elapsed;
    input >> elapsed >> context.ground.present >> context.ground.height;
    context.elapsed = static_cast<tfx::simulation::timer_ticks>(elapsed);
    context.gear_height = read_float(input);
    context.metres_per_second = read_float(input);
    context.turn_metres_per_second = read_float(input);
    context.reported_speed = read_float(input);
    input >> context.stall_metres_per_second >> context.low_speed_reference >> context.rollout_speed_threshold >> context.fixed_speed >> context.gear_down >> context.pitch_inhibited >> context.pitch_down >> context.surface_type >> context.arrestor_hook;
    if(action & 1) tfx::flight::update_simple_stall_attitude(state,context);
    if(action & 2) tfx::flight::update_simple_ground(state,context);
    std::array<uint64_t,32> values;
    unsigned int cursor{0};
    for(auto const *basis : {&state.body,&state.visible}) for(unsigned int row{0}; row != 3; ++row) {
      values[cursor++] = static_cast<uint32_t>(basis->right[row]);
      values[cursor++] = static_cast<uint32_t>(basis->down[row]);
      values[cursor++] = static_cast<uint32_t>(basis->forward[row]);
    }
    for(auto const value : state.rotation_remainder) values[cursor++] = std::bit_cast<uint32_t>(value);
    values[cursor++] = std::bit_cast<uint32_t>(state.position_y);
    values[cursor++] = static_cast<uint32_t>(state.stall_pitch);
    values[cursor++] = static_cast<uint32_t>(state.landing_angles.x);
    values[cursor++] = static_cast<uint32_t>(state.landing_angles.y);
    values[cursor++] = state.landed;
    values[cursor++] = state.previous_landed;
    values[cursor++] = state.low_speed_ground_contact;
    values[cursor++] = static_cast<uint32_t>(state.collision);
    values[cursor++] = state.arrested;
    values[cursor++] = std::bit_cast<uint64_t>(state.indicated_speed);
    values[cursor++] = std::bit_cast<uint64_t>(state.low_speed_reference);
    if(cursor != values.size()) throw std::runtime_error{"Simple ground output dimensions differ"};
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native Simple ground fixture differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no Simple ground cases"};
  if(argc == 2) std::cout << cases << " native Simple ground fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
