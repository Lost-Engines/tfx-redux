#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "actors/visibility.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"actor_visibility_probe [FIXTURE]"};
  std::ifstream file;
  if(argc == 2) { file.open(argv[1]);if(!file) throw std::runtime_error{"cannot open actor visibility fixture"}; }
  auto &input{file.is_open() ? file : std::cin};
  vec3<int32_t> position, reference;
  unsigned int cases{0};
  while(input >> position.x >> position.y >> position.z >> reference.x >> reference.y >> reference.z) {
    bool const air{tfx::actors::aircraft_in_draw_region(position,reference)}, surface{tfx::actors::surface_in_draw_region(position,reference)};
    if(file.is_open()) {
      bool expected_air, expected_surface;
      if(!(input >> expected_air >> expected_surface) || air != expected_air || surface != expected_surface) throw std::runtime_error{"native actor draw region differs at case " + std::to_string(cases)};
    } else std::cout << air << ' ' << surface << '\n';
    ++cases;
  }
  if(!cases || !input.eof()) throw std::runtime_error{"incomplete actor visibility input"};
  if(file.is_open()) std::cout << cases << " original actor draw regions passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
