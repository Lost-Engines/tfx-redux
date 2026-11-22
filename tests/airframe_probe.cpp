#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: airframe_probe TFX.DAT"};
  auto const profiles{tfx::flight::load_airframes(tfx::resources::read_file(argv[1]))};
  for(auto const &profile : profiles) {
    for(auto const value : {profile.stores.moment_a, profile.stores.moment_b, profile.stores.fuel_capacity}) std::cout << std::bit_cast<uint64_t>(value) << ' ';
    for(auto const &mount : profile.mounts) {
      for(unsigned int axis{0}; axis != 3; ++axis) std::cout << std::bit_cast<uint64_t>(mount[axis]) << ' ';
    }
    std::cout << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
