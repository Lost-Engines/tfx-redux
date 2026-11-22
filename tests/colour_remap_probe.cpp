#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/camera.h"
#include "render/colour_remap.h"
#include "render/point_projection.h"
#include "world/assets.h"

int main(int const argc, char const *const argv[]) try {
  if(argc == 3 && std::string_view{argv[1]} == "--assets") {
    tfx::resources::archive const game{tfx::resources::find_game_archive(argv[2])};
    tfx::world::assets const assets{game};
    for(unsigned int row{0}; row != tfx::render::colour_map_count; ++row) {
      auto const &colours{assets.model_colours(static_cast<uint8_t>(row))};
      std::cout.write(reinterpret_cast<char const *>(colours.data()), colours.size());
    }
    return EXIT_SUCCESS;
  }
  if(argc >= 2 && std::string_view{argv[1]} == "--placement") {
    std::ifstream fixture;
    if(argc == 3) fixture.open(argv[2]);
    if(argc > 3 || (argc == 3 && !fixture)) throw std::runtime_error{"expected --placement [FILE]"};
    auto &input{argc == 3 ? fixture : std::cin};
    bool fixed;
    unsigned int cases{0};
    while(input >> fixed) {
      mat3<int32_t> matrix;
      vec3<int32_t> camera;
      vec3<int32_t> point;
      for(auto &value : matrix.data) input >> value;
      input >> camera.x >> camera.y >> camera.z >> point.x >> point.y >> point.z;
      auto const projection{tfx::render::original_camera(camera, matrix, {159, 99}).projector()};
      auto const raw{fixed ? projection.camera_fixed(point) : projection.camera_integer(point)};
      int32_t const values[]{raw.x, raw.y, raw.z, tfx::render::distance_colour_row(raw.z)};
      if(argc == 3) {
        for(auto const value : values) {
          int32_t expected;
          if(!(input >> expected) || value != expected) throw std::runtime_error{"native placed-model colour row mismatch at case " + std::to_string(cases)};
        }
      } else {
        for(auto const value : values) std::cout << value << ' ';
        std::cout << '\n';
      }
      ++cases;
    }
    return cases ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE] or --assets DATA_DIRECTORY"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  int32_t depth;
  while(input >> depth) {
    unsigned int const row{tfx::render::distance_colour_row(depth)};
    if(argc == 2) {
      unsigned int expected;
      if(!(input >> expected) || expected != row) throw std::runtime_error{"native model colour row mismatch at case " + std::to_string(cases)};
    } else {
      std::cout << row << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
