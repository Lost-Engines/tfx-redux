#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "world/scene.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"world_probe DATA_DIRECTORY ENVIRONMENT"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::world::assets assets{game};
  tfx::world::environment const environment{game, argv[2]};
  vec3<int32_t> previous;
  vec3<int32_t> current;
  while(std::cin >> previous.x >> previous.y >> previous.z >> current.x >> current.y >> current.z) {
    tfx::world::scene scene{environment, assets};
    auto const contact{scene.collide_terrain(previous, current)};
    std::cout << contact.collisions << ' ' << contact.volume_hit << ' ' << contact.surface_present << ' ' << contact.surface_height.value_or(0) << ' ' << contact.zone << ' ' << contact.player_zone << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
