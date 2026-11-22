#include <cstdlib>
#include <iomanip>
#include <iostream>
#include "presentation/pilots.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"name_probe TFX_DAT"};
  tfx::presentation::pilot_assets const assets{tfx::resources::read_file(argv[1])};
  std::string fallback;
  unsigned int count{0};
  unsigned int control{0};
  while(std::cin >> std::quoted(fallback) >> control >> count) {
    tfx::presentation::name_entry field{assets, fallback};
    for(unsigned int index{0}; index != count; ++index) {
      unsigned int scan{0};
      std::cin >> scan;
      field.press(static_cast<uint8_t>(scan), control != 0);
    }
    std::cout << std::quoted(field.value()) << ' ' << field.qualified() << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
