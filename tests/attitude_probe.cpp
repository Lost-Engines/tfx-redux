#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include "math/attitude.h"
#include "math/native.h"

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 2) throw std::runtime_error{"attitude probe requires rotate, normalise or angles"};
  std::string const mode{argv[1]};
  if(mode == "plane") {
    int x{0};
    int y{0};
    unsigned int rotation{0};
    while(std::cin >> x >> y >> rotation) {
      auto const point{tfx::math::rotate_words({static_cast<int16_t>(x), static_cast<int16_t>(y)}, static_cast<tfx::math::angle>(rotation))};
      std::cout << point.x << ' ' << point.y << '\n';
    }
    return EXIT_SUCCESS;
  }
  if(mode == "direction") {
    int y{0};
    int x{0};
    while(std::cin >> y >> x) std::cout << tfx::math::direction_angle(static_cast<int16_t>(y), static_cast<int16_t>(x)) << '\n';
    return EXIT_SUCCESS;
  }
  unsigned int operation{0};
  unsigned int amount{0};
  while(std::cin >> operation >> amount) {
    tfx::math::attitude basis;
    for(unsigned int row{0}; row != 3; ++row) std::cin >> basis.right[row] >> basis.down[row] >> basis.forward[row];
    if(mode == "rotate") {
      basis.rotate(static_cast<tfx::math::rotation_axis>(operation), static_cast<uint16_t>(amount));
    } else if(mode == "normalise") {
      basis.normalise();
    } else if(mode == "angles") {
      auto const angles{basis.angles()};
      std::cout << angles.x << ' ' << angles.y << ' ' << angles.z << '\n';
      continue;
    } else {
      throw std::runtime_error{"unknown attitude probe mode"};
    }
    for(unsigned int row{0}; row != 3; ++row) std::cout << basis.right[row] << ' ' << basis.down[row] << ' ' << basis.forward[row] << ' ';
    std::cout << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
