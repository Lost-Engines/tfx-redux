#include <iostream>
#include <stdexcept>
#include "world/environment.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"environment probe requires original data directory"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::string name;
  while(std::cin >> name) {
    tfx::world::lighting_configuration const environment{game, name};
    std::cout << environment.palette_name;
    for(auto const &palette : environment.upper_palette_names) std::cout << ' ' << palette;
    for(auto const altitude : environment.palette_altitudes) std::cout << ' ' << altitude;
    std::cout << ' ' << environment.infrared_palette_name << ' ' << environment.schematic_name << ' ' << environment.status << '\n';
  }
}
