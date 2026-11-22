#include <cstdlib>
#include <iostream>
#include "resources/executable_tables.h"
int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"cursor_probe EXECUTABLE"};
  tfx::resources::executable_tables const tables{argv[1]};
  unsigned int style{0};
  unsigned int active{0};
  int x{0};
  int y{0};
  while(std::cin >> style >> active >> x >> y) {
    tfx::render::framebuffer output;
    output.pixels.fill(99);
    auto const &pointer{tables.pointers.at(style)};
    tfx::presentation::draw_pointer(output, active ? pointer.active : pointer.idle, {static_cast<int16_t>(x), static_cast<int16_t>(y)});
    std::cout.write(reinterpret_cast<char const *>(output.pixels.data()), static_cast<std::streamsize>(output.pixels.size()));
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
