#include <array>
#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "flight/simple_controls.h"

namespace {

float read_float(std::istream &input) {
  uint32_t bits;
  input >> bits;
  return std::bit_cast<float>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open Simple control fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  int32_t speed;
  while(input >> speed) {
    tfx::math::attitude orientation;
    tfx::flight::simple_control_context context{
      .fixed_speed{speed},
      .orientation{orientation},
    };
    context.metres_per_second = read_float(input);
    input >> context.maximum_g >> context.minimum_g >> context.stall_metres_per_second;
    context.coordinated_turn_rate = read_float(input);
    for(unsigned int row{0}; row != 3; ++row) input >> orientation.right[row] >> orientation.down[row] >> orientation.forward[row];
    unsigned int elapsed;
    unsigned int source;
    input >> elapsed >> source >> context.joystick.x >> context.joystick.y >> context.keys.up >> context.keys.left >> context.keys.down >> context.keys.right >> context.landed >> context.low_speed_ground_contact >> context.pitch_inhibited >> context.alt;
    context.elapsed = static_cast<tfx::simulation::timer_ticks>(elapsed);
    context.source = static_cast<tfx::flight::control_source>(source);
    tfx::flight::simple_control_state state;
    for(auto &value : state.angular_rate) value = read_float(input);
    state.negative_pitch_limit = read_float(input);
    state.positive_pitch_limit = read_float(input);
    state.roll_limit = read_float(input);
    auto const keys{tfx::flight::update_simple_controls(state, context)};
    std::array<uint32_t, 10> const values{
      std::bit_cast<uint32_t>(state.angular_rate.x), std::bit_cast<uint32_t>(state.angular_rate.y), std::bit_cast<uint32_t>(state.angular_rate.z),
      std::bit_cast<uint32_t>(state.negative_pitch_limit), std::bit_cast<uint32_t>(state.positive_pitch_limit), std::bit_cast<uint32_t>(state.roll_limit),
      keys.up, keys.left, keys.down, keys.right,
    };
    if(argc == 2) {
      for(auto const value : values) {
        uint32_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native Simple control fixture differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no Simple control cases"};
  if(argc == 2) std::cout << cases << " native Simple control fixtures agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
