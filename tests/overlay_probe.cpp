#include <cstdlib>
#include <iostream>
#include <map>
#include "resources/archive.h"
#include "resources/compiled_overlay.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: overlay_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::map<std::string, tfx::resources::compiled_overlay> overlays;
  std::string name;
  int pan{0};
  int vertical{0};
  while(std::cin >> name >> pan >> vertical) {
    if(!overlays.contains(name)) overlays.emplace(name, tfx::resources::compiled_overlay{game.read(name)});
    auto const frame{overlays.at(name).image(pan, vertical)};
    std::cout.write(reinterpret_cast<char const *>(frame.image.pixels.data()), frame.image.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(frame.coverage.pixels.data()), frame.coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
